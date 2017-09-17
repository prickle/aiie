#include <Arduino.h>
#include <ff.h> // uSDFS
#include <SPI.h>
#include <EEPROM.h>
#include <TimeLib.h>
#include <TimerOne.h>
#include "bios.h"
#include "cpu.h"
#include "applevm.h"
#include "teensy-display.h"
#include "teensy-keyboard.h"
#include "teensy-speaker.h"
#include "teensy-paddles.h"
#include "teensy-filemanager.h"

#define RESETPIN 39
#define BATTERYPIN A19
#define SPEAKERPIN A21

#include "globals.h"
#include "teensy-crash.h"

volatile float nextInstructionMicros;
volatile float startMicros;

FATFS fatfs;      /* File system object */
BIOS bios;

enum {
  D_NONE        = 0,
  D_SHOWFPS     = 1,
  D_SHOWMEMFREE = 2,
  D_SHOWPADDLES = 3,
  D_SHOWPC      = 4,
  D_SHOWCYCLES  = 5,
  D_SHOWBATTERY = 6,
  D_SHOWTIME    = 7
};
uint8_t debugMode = D_NONE;

static   time_t getTeensy3Time() {  return Teensy3Clock.get(); }

#define ESP_TXD 51
#define ESP_CHPD 52
#define ESP_RST 53
#define ESP_RXD 40
#define ESP_GPIO0 41
#define ESP_GPIO2 42

void setup()
{
  Serial.begin(230400);

  /* while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  Serial.println("hi");
  */
  delay(100); // let the serial port connect if it's gonna

  enableFaultHandler();

  // set the Time library to use Teensy 3.0's RTC to keep time
  setSyncProvider(getTeensy3Time);
  delay(100); // don't know if we need this
  if (timeStatus() == timeSet) {
    Serial.println("RTC set from Teensy");
  } else {
    Serial.println("Error while setting RTC");
  }

  TCHAR *device = (TCHAR *)_T("0:/");
  f_mount (&fatfs, device, 0);      /* Mount/Unmount a logical drive */

  pinMode(RESETPIN, INPUT);
  digitalWrite(RESETPIN, HIGH);

  analogReference(EXTERNAL); // 3.3v external, or 1.7v internal. We need 1.7 internal for the battery level, which means we're gonna have to do something about the paddles :/  
  analogReadRes(8); // We only need 8 bits of resolution (0-255) for battery & paddles
  analogReadAveraging(4); // ?? dunno if we need this or not.
  analogWriteResolution(12);
  
  pinMode(SPEAKERPIN, OUTPUT); // analog speaker output, used as digital volume control
  pinMode(BATTERYPIN, INPUT);

  Serial.println("creating virtual hardware");
  g_speaker = new TeensySpeaker(SPEAKERPIN);

  Serial.println(" fm");
  // First create the filemanager - the interface to the host file system.
  g_filemanager = new TeensyFileManager();

  // Construct the interface to the host display. This will need the
  // VM's video buffer in order to draw the VM, but we don't have that
  // yet. 
  Serial.println(" display");
  g_display = new TeensyDisplay();

  // Next create the virtual CPU. This needs the VM's MMU in order to
  // run, but we don't have that yet.
  Serial.println(" cpu");
  g_cpu = new Cpu();

  // Create the virtual machine. This may read from g_filemanager to
  // get ROMs if necessary.  (The actual Apple VM we've built has them
  // compiled in, though.) It will create its virutal hardware (MMU,
  // video driver, floppy, paddles, whatever).
  Serial.println(" vm");
  g_vm = new AppleVM();

  // Now that the VM exists and it has created an MMU, we tell the CPU
  // how to access memory through the MMU.
  Serial.println(" [setMMU]");
  g_cpu->SetMMU(g_vm->getMMU());

  // And the physical keyboard needs hooks in to the virtual keyboard...
  Serial.println(" keyboard");
  g_keyboard = new TeensyKeyboard(g_vm->getKeyboard());

  Serial.println(" paddles");
  g_paddles = new TeensyPaddles(A23, A24, 1, 1);

  // Now that all the virtual hardware is glued together, reset the VM
  Serial.println("Resetting VM");
  g_vm->Reset();

  g_display->redraw();
//  g_display->blit();

  Serial.println("Reading prefs");
  readPrefs(); // read from eeprom and set anything we need setting

  Serial.println("free-running");

  startMicros = 0;
  nextInstructionMicros = micros();

  // Debugging: insert a disk on startup...
  //  ((AppleVM *)g_vm)->insertDisk(0, "/A2DISKS/UTIL/mock2dem.dsk", false);
  //  ((AppleVM *)g_vm)->insertDisk(0, "/A2DISKS/JORJ/disk_s6d1.dsk", false);
  // ((AppleVM *)g_vm)->insertDisk(0, "/A2DISKS/GAMES/ALIBABA.DSK", false);

  pinMode(56, OUTPUT);
  pinMode(57, OUTPUT);

  Timer1.initialize(3);
  Timer1.attachInterrupt(runCPU);
  Timer1.start();
}

// FIXME: move these memory-related functions elsewhere...

// This only gives you an estimated free mem size. It's not perfect.
uint32_t FreeRamEstimate()
{
  uint32_t stackTop;
  uint32_t heapTop;

  // current position of the stack.
  stackTop = (uint32_t) &stackTop;

  // current position of heap.
  void* hTop = malloc(1);
  heapTop = (uint32_t) hTop;
  free(hTop);

  // The difference is the free, available ram.
  return stackTop - heapTop;
}

#include "malloc.h"

int heapSize(){
  return mallinfo().uordblks;
}

void biosInterrupt()
{
  Timer1.stop();

  // wait for the interrupt button to be released
  while (digitalRead(RESETPIN) == LOW)
    ;

  // invoke the BIOS
  if (bios.runUntilDone()) {
    // if it returned true, we have something to store persistently in EEPROM.
    writePrefs();
  }

  // if we turned off debugMode, make sure to clear the debugMsg
  if (debugMode == D_NONE) {
    g_display->debugMsg("");
  }

  // clear the CPU next-step counters
  g_cpu->cycles = 0;
  nextInstructionMicros = micros();
  startMicros = micros();

  // Force the display to redraw
  ((AppleDisplay*)(g_vm->vmdisplay))->modeChange();

  // Poll the keyboard before we start, so we can do selftest on startup
  g_keyboard->maintainKeyboard();

  Timer1.start();
}

//bool debugState = false;
//bool debugLCDState = false;

void runCPU()
{
  if (micros() >= nextInstructionMicros) {
    // Debugging: to watch when the CPU is triggered...
    //debugState = !debugState;
    //    digitalWrite(56, debugState);
    
    g_cpu->Run(24);
    
    // These are timing-critical, for the audio and paddles.
    // There's also a keyboard repeat in here that hopefully is 
    // minimal overhead...
    g_speaker->beginMixing();
    ((AppleVM *)g_vm)->cpuMaintenance(g_cpu->cycles);
    g_speaker->maintainSpeaker(g_cpu->cycles);

    // The CPU of the Apple //e ran at 1.023 MHz. Adjust when we think
    // the next instruction should run based on how long the execution
    // was ((1000/1023) * numberOfCycles) - which is about 97.8%.
    nextInstructionMicros = startMicros + (float)g_cpu->cycles * 0.978;
  }
}

void loop()
{
  if (digitalRead(RESETPIN) == LOW) {
    // This is the BIOS interrupt. We immediately act on it.
    biosInterrupt();
  } 

  ((AppleVM*)g_vm)->disk6->fillDiskBuffer();

  g_keyboard->maintainKeyboard();

  //debugLCDState = !debugLCDState;
  //digitalWrite(57, debugLCDState);

  doDebugging();

  // Only redraw if the CPU is caught up; and then we'll suspend the
  // CPU to draw a full frame.

  // Note that this breaks audio, b/c it's real-time and requires the
  // CPU running to change the audio line's value. So we need to EITHER
  //
  //   - delay the audio line by at least the time it takes for one
  //     display update, OR
  //   - lock display updates so the CPU can update the memory, but we
  //     keep drawing what was going to be displayed
  // 
  // The Timer1.stop()/start() is bad. Using it, the display doesn't
  // tear; but the audio is also broken. Taking it out, audio is good
  // but the display tears.

  Timer1.stop();
  g_vm->vmdisplay->needsRedraw();
  AiieRect what = g_vm->vmdisplay->getDirtyRect();
  g_vm->vmdisplay->didRedraw();
  g_display->blit(what);
  Timer1.start();

  static unsigned long nextBattCheck = 0;
  static int batteryLevel = 0; // static for debugging code! When done
			       // debugging, this can become a local
			       // in the appropriate block below
  if (millis() >= nextBattCheck) {
    // FIXME: what about rollover?
    nextBattCheck = millis() + 3 * 1000; // check every 30 seconds

    // This is a bit disruptive - but the external 3.3v will drop along with the battery level, so we should use the more stable (I hope) internal 1.7v.
    // The alternative is to build a more stable buck/boost regulator for reference...
    analogReference(INTERNAL);
    batteryLevel = analogRead(BATTERYPIN);
    analogReference(EXTERNAL);

    /* LiIon charge to a max of 4.2v; and we should not let them discharge below about 3.5v.
     *  With a resistor voltage divider of Z1=39k, Z2=10k we're looking at roughly 20.4% of 
     *  those values: (10/49) * 4.2 = 0.857v, and (10/49) * 3.5 = 0.714v. Since the external 
     *  voltage reference flags as the battery drops, we can't use that as an absolute 
     *  reference. So using the INTERNAL 1.1v reference, that should give us a reasonable 
     *  range, in theory; the math shows the internal reference to be about 1.27v (assuming 
     *  the resistors are indeed 39k and 10k, which is almost certainly also wrong). But 
     *  then the high end would be 172, and the low end is about 142, which matches my 
     *  actual readings here very well.
     *  
     *  Actual measurements: 
     *    3.46v = 144 - 146
     *    4.21v = 172
     */
#if 1
    Serial.print("battery: ");
    Serial.println(batteryLevel);
#endif
    
    if (batteryLevel < 146)
      batteryLevel = 146;
    if (batteryLevel > 168)
      batteryLevel = 168;

    batteryLevel = map(batteryLevel, 146, 168, 0, 100);
    g_display->drawBatteryStatus(batteryLevel);
  }
}

void doDebugging()
{
  char buf[25];
  switch (debugMode) {
  case D_SHOWFPS:
    // display some FPS data
    static uint32_t startAt = millis();
    static uint32_t loopCount = 0;
    loopCount++;
    time_t lenSecs;
    lenSecs = (millis() - startAt) / 1000;
    if (lenSecs >= 5) {
      sprintf(buf, "%lu FPS", loopCount / lenSecs);
      g_display->debugMsg(buf);
      startAt = millis();
      loopCount = 0;
    }
    break;
  case D_SHOWMEMFREE:
    sprintf(buf, "%lu %u", FreeRamEstimate(), heapSize());
    g_display->debugMsg(buf);
    break;
  case D_SHOWPADDLES:
    sprintf(buf, "%u %u", g_paddles->paddle0(), g_paddles->paddle1());
    g_display->debugMsg(buf);
    break;
  case D_SHOWPC:
    sprintf(buf, "%X", g_cpu->pc);
    g_display->debugMsg(buf);
    break;
  case D_SHOWCYCLES:
    sprintf(buf, "%lX", g_cpu->cycles);
    g_display->debugMsg(buf);
    break;
  case D_SHOWBATTERY:
    sprintf(buf, "BAT %d", analogRead(BATTERYPIN));
    g_display->debugMsg(buf);
    break;
  case D_SHOWTIME:
    sprintf(buf, "%.2d:%.2d:%.2d", hour(), minute(), second());
    g_display->debugMsg(buf);
    break;
  }
}

typedef struct _prefs {
  uint32_t magic;
  int16_t volume;
} prefs;

// Fun trivia: the Apple //e was in production from January 1983 to
// November 1993. And the 65C02 in them supported weird BCD math modes.
#define MAGIC 0x01831093

void readPrefs()
{
  prefs p;
  uint8_t *pp = (uint8_t *)&p;

  Serial.println("reading prefs");

  for (uint8_t i=0; i<sizeof(prefs); i++) {
    *pp++ = EEPROM.read(i);
  }

  if (p.magic == MAGIC) {
    // looks valid! Use it.
    Serial.println("prefs valid! Restoring volume");
    if (p.volume > 15) {
      p.volume = 15;
    }
    if (p.volume < 0) {
      p.volume = 0;
    }

    g_volume = p.volume;
    return;
  }

  // use defaults
  g_volume = 0;
}

void writePrefs()
{
  Serial.println("writing prefs");
  Timer1.stop();

  prefs p;
  uint8_t *pp = (uint8_t *)&p;

  p.magic = MAGIC;
  p.volume = g_volume;

  for (uint8_t i=0; i<sizeof(prefs); i++) {
    EEPROM.write(i, *pp++);
  }

  Timer1.start();
}

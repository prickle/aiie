#include <Arduino.h>
#include "teensy-usb-keyboard.h"
#include <USBHost_t36.h>
#include <RingBufferJB.h>

USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
KeyboardController keyboard1(myusb);

RingBufferJB bufferUsb(10); // 10 keys should be plenty, right?


char ps2KeyLayout[102] = 
{
    0,   0,   0,   0, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
  'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
  'y', 'z', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', RET, ESC,
  DEL, TAB, ' ', '-', '=', '[', ']','\\', '?', ';','\'', '`', ',', '.',
  '/',LOCK,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  SYSRQ, 0,   0,   0,   0,   0,   0,   0,   0,RARR,LARR,DARR,UARR,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,  LA,  RA
};

static uint8_t shiftedNumber[] = { '<', // ,
				   '_', // -
				   '>', // .
				   '?', // /
				   ')', // 0
				   '!', // 1
				   '@', // 2
				   '#', // 3
				   '$', // 4
				   '%', // 5
				   '^', // 6
				   '&', // 7
				   '*', // 8
				   '(', // 9
				   0,   // (: is not a key)
				   ':'  // ;
};

//bool buffered;
bool leftShiftPressed;
bool rightShiftPressed;
bool ctrlPressed;
bool capsLock;
bool leftApplePressed;
bool rightApplePressed;
int8_t numPressed;
bool buffered;
VMKeyboard *vmKeyboard;


TeensyUsbKeyboard::TeensyUsbKeyboard(VMKeyboard *k) : PhysicalKeyboard(k)
{
  myusb.begin();
  keyboard1.attachPress(onPress);
  keyboard1.attachRelease(onRelease);
  //keyboard1.rawOnly(true);

  leftShiftPressed = false;
  rightShiftPressed = false;
  ctrlPressed = false;
  capsLock = true;
  leftApplePressed = false;
  rightApplePressed = false;
  buffered = false;
  vmKeyboard = k;
  
  numPressed = 0;
}

TeensyUsbKeyboard::~TeensyUsbKeyboard()
{
}

void onPress(int unicode)
{
  uint8_t key = keyboard1.getOemKey();
  uint8_t modifiers = keyboard1.getModifiers();
  if (key > 101) key = 101;
  if (buffered) {
	  pressedKey(ps2KeyLayout[key], modifiers);
  } else {
	  vmKeyboard->keyDepressed(ps2KeyLayout[key], modifiers);
  }
}

void onRelease(int unicode)
{
  uint8_t key = keyboard1.getOemKey();
  uint8_t modifiers = keyboard1.getModifiers();
  if (key > 101) key = 101;
  if (buffered) {
	  releasedKey(ps2KeyLayout[key], modifiers);
  } else {
	  vmKeyboard->keyReleased(ps2KeyLayout[key], modifiers);
  }
}

void pressedKey(uint8_t key, uint8_t mod)
{
  numPressed++;
  if (key != SYSRQ && key & 0x80) {
    // it's a modifier key.
    switch (key) {
    case _CTRL:
      ctrlPressed = 1;
      break;
    case LSHFT:
      leftShiftPressed = 1;
      break;
    case RSHFT:
      rightShiftPressed = 1;
      break;
    case LOCK:
      capsLock = !capsLock;
      break;
    case LA:
      leftApplePressed = 1;
      break;
    case RA:
      rightApplePressed = 1;
      break;
    }
    return;
  }

  if (key == ' ' || key == DEL || key == ESC || key == RET || key == TAB || key == SYSRQ) {
	  //Serial.println((int)key);

    bufferUsb.addByte(key);
    return;
  }

  if (key >= 'a' &&
      key <= 'z') {
    if (ctrlPressed) {
      bufferUsb.addByte(key - 'a' + 1);
      return;
    }
    if (leftShiftPressed || rightShiftPressed || capsLock) {
      bufferUsb.addByte(key - 'a' + 'A');
      return;
    }
    bufferUsb.addByte(key);
    return;
  }

  // FIXME: can we control-shift?
  if (key >= ',' && key <= ';') {
    if (leftShiftPressed || rightShiftPressed) {
      bufferUsb.addByte(shiftedNumber[key - ',']);
      return;
    }
    bufferUsb.addByte(key);
    return;
  }

  if (leftShiftPressed || rightShiftPressed) {
    uint8_t ret = 0;
    switch (key) {
    case '=':
      ret = '+';
      break;
    case '[':
      ret = '{';
      break;
    case ']':
      ret = '}';
      break;
    case '\\':
      ret = '|';
      break;
    case '\'':
      ret = '"';
      break;
    case '`':
      ret = '~';
      break;
    }
    if (ret) {
      bufferUsb.addByte(ret);
      return;
    }
  }

  // Everything else falls through.
  bufferUsb.addByte(key);
}

void releasedKey(uint8_t key, uint8_t mod)
{
  numPressed--;
  if (key & 0x80) {
    // it's a modifier key.
    switch (key) {
    case _CTRL:
      ctrlPressed = 0;
      break;
    case LSHFT:
      leftShiftPressed = 0;
      break;
    case RSHFT:
      rightShiftPressed = 0;
      break;
    case LA:
      leftApplePressed = 0;
      break;
    case RA:
      rightApplePressed = 0;
      break;
    }
  }
}

bool TeensyUsbKeyboard::kbhit()
{
  if (!buffered) {
    bufferUsb.clear();
    buffered = true;
  }
  myusb.Task();
  // For debugging: also allow USB serial to act as a keyboard
  if (Serial.available()) {
    bufferUsb.addByte(Serial.read());
  }

  return bufferUsb.hasData();
}

uint8_t TeensyUsbKeyboard::read()
{
  if (bufferUsb.hasData()) {
    return bufferUsb.consumeByte();
  }

  return 0;
}

// This is a non-buffered interface to the physical keyboard, as used
// by the VM.
void TeensyUsbKeyboard::maintainKeyboard()
{
//	Serial.println("maintain");
  buffered = false;
  myusb.Task();
  // For debugging: also allow USB serial to act as a keyboard
  if (Serial.available()) {
    int c = Serial.read();
    vmkeyboard->keyDepressed(c);
    vmkeyboard->keyReleased(c);
  }
}

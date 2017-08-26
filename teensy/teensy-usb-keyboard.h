#ifndef __TEENSY_USB_KEYBOARD_H
#define __TEENSY_USB_KEYBOARD_H

#include "physicalkeyboard.h"

void onPress(int unicode);
void onRelease(int unicode);
void pressedKey(uint8_t key, uint8_t mod);
void releasedKey(uint8_t key, uint8_t mod);

class TeensyUsbKeyboard : public PhysicalKeyboard {
 public:
  TeensyUsbKeyboard(VMKeyboard *k);
  virtual ~TeensyUsbKeyboard();

  // Interface used by the VM...
  virtual void maintainKeyboard();

  // Interface used by the BIOS...
  virtual bool kbhit();
  virtual uint8_t read();

};

#endif

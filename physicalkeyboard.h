#ifndef __PHYSICALKEYBOARD_H
#define __PHYSICALKEYBOARD_H

#include <stdint.h>

#include "vmkeyboard.h"

#define ESC 0x1B
#define DEL 0x7F
#define RET 0x0D
#define TAB 0x09
#define LARR 0x08 // control-H
#define RARR 0x15 // control-U
#define DARR 0x0A
#define UARR 0x0B
#define SYSRQ 0x87

// Virtual keys
#define _CTRL  0x81
#define LSHFT 0x82
#define RSHFT 0x83
#define LOCK  0x84 // caps lock
#define LA    0x85 // left (open) apple, aka paddle0 button
#define RA    0x86 // right (closed) apple aka paddle1 button

//USB modifiers
#define USB_LEFT_CTRL   0x01
#define USB_LEFT_SHIFT  0x02
#define USB_LEFT_ALT    0x04
#define USB_LEFT_GUI    0x08
#define USB_RIGHT_CTRL  0x10
#define USB_RIGHT_SHIFT 0x20
#define USB_RIGHT_ALT   0x40
#define USB_RIGHT_GUI   0x80


class PhysicalKeyboard {
 public:
  PhysicalKeyboard(VMKeyboard *k) { this->vmkeyboard = k; }
  virtual ~PhysicalKeyboard() {};

  virtual void maintainKeyboard() = 0;
  virtual bool kbhit() = 0;
  virtual uint8_t read() = 0;

 protected:
  VMKeyboard *vmkeyboard;
};

#endif

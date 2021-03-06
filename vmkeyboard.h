#ifndef __VMKEYBOARD_H
#define __VMKEYBOARD_H

#include <stdint.h>

class VMKeyboard {
 public:
  virtual ~VMKeyboard() {}

  virtual void keyDepressed(uint8_t k) = 0;
  virtual void keyReleased(uint8_t k) = 0;
  virtual void keyDepressed(uint8_t k, uint8_t m) = 0;
  virtual void keyReleased(uint8_t k, uint8_t m) = 0;
  virtual void maintainKeyboard(uint32_t cycleCount) = 0;
};

#endif

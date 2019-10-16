// -*- c++ -*-

#ifndef PEOS2_X86_H
#define PEOS2_X86_H

#include <stdint.h>

inline void outb(uint16_t port, uint8_t value) {
  // dN = 8 bits are passed as immediate, while larger values are set in dx
  asm volatile ("out %0, %1" : : "dN" (port), "a" (value));
}

inline void outw(uint16_t port, uint16_t value) {
  asm volatile ("out %0, %1" : : "dN" (port), "ax" (value));
}

inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  asm volatile ("in %0, %1" : "=a" (ret) : "dN" (port));
  return ret;
}


#endif // !PEOS2_X86_H

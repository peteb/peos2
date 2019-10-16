// -*- c++ -*-

#ifndef PEOS2_X86_H
#define PEOS2_X86_H

#include <stdint.h>

#define CR0_PE 0x00000001
#define CR0_MP 0x00000002
#define CR0_EM 0x00000004
#define CR0_TS 0x00000008
#define CR0_ET 0x00000010
#define CR0_NE 0x00000020
#define CR0_WP 0x00010000
#define CR0_AM 0x00040000
#define CR0_NW 0x20000000
#define CR0_CD 0x40000000
#define CR0_PG 0x80000000

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

inline uint32_t cr0() {
  uint32_t ret;
  asm volatile ("mov %0, cr0" : "=r" (ret) :);
  return ret;
}

bool a20_enabled();
void enable_a20();

#endif // !PEOS2_X86_H

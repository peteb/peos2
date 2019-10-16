#include "screen.h"
#include "x86.h"
#include "support/string.h"


extern "C" void main() {
  clear_screen();
  puts("Welcome!\n");

  // Disable NMIs
  outb(0x70, inb(0x70) | 0x80);
  asm volatile ("cli");

  if (!a20_enabled()) {
    enable_a20();
  }
}

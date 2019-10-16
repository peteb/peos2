#include "x86.h"
#include "assert.h"

bool a20_enabled() {
  if (cr0() & CR0_PE) {
    // Protected mode is enabled, so check whether the addresses below are aliases
    unsigned volatile *ptr1 = reinterpret_cast<unsigned *>(0x112345);
    unsigned volatile *ptr2 = reinterpret_cast<unsigned *>(0x012345);
    *ptr1 = 0x1234;
    *ptr2 = 0x4321;
    return *ptr1 != *ptr2;
  }
  else {
    assert(!"implement real mode support");
    return false;
  }
}

void enable_a20() {
  // Fast A20
  outb(0x92, inb(0x92) | 0x2);

  // TODO: implement other alternatives of setting A20
}

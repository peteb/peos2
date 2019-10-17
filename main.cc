#include "screen.h"
#include "x86.h"
#include "support/string.h"
#include "protected_mode.h"

extern "C" void main() {
  clear_screen();
  puts("Entering protected mode...\n");

  enter_protected_mode();

  // TODO: setup a new stack?
  puts("Updated GDT\n");

  uint32_t esp = 0;
  asm volatile("mov %0, esp" : "=m" (esp) :);

  puts(p2::fixed_string<32>()
       .append("ESP=").append(esp, 8, 16, '0').append("\n")
       .str());
}

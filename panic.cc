#include "panic.h"
#include "screen.h"

void panic(const char *explanation) {
  print("PANIC! ");
  print(explanation);

  while (true) {
    // This should trigger a GPF if run in a user space process
    asm volatile("cli\nhlt");
  }
}

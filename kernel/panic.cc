#include "support/panic.h"
#include "screen.h"
#include "process.h"
#include "syscall_utils.h"
#include "debug.h"

void panic(const char *explanation) {
  asm volatile("xchg bx, bx");

  if (proc_current_pid()) {
    dbg_puts(sys, "panic: %s", explanation);
    kill_caller();
  }
  else {
    print("PANIC! ");
    print(explanation);
  }

  while (true) {
    asm volatile("cli");
    // NB: not HLTing; bochs freezes then
  }
}

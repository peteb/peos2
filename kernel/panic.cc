#include "panic.h"
#include "screen.h"
#include "process.h"
#include "syscall_utils.h"
#include "debug.h"

void panic(const char *explanation) {
  if (proc_current_pid()) {
    dbg_puts(sys, "panic: %s", explanation);
    kill_caller();
  }
  else {
    print("PANIC! ");
    print(explanation);
  }

  while (true) {
    asm volatile("cli\nhlt");
  }
}

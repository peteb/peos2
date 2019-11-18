#include "support/panic.h"
#include "screen.h"
#include "process.h"
#include "syscall_utils.h"
#include "syscall_decls.h"
#include "debug.h"
#include "x86.h"

void panic(const char *explanation) {
  asm volatile("xchg bx, bx");  // "magical breakpoint"

  uint32_t cs = 0;
  asm volatile("xor eax, eax\n"
               "mov eax, cs\n"
               "mov %0, eax" :"=a"(cs));

  if (SEG_RPL(cs) == 3) {
    // This is *inside* a user space process, not in an interrupt
    // handler! So we cannot use dbg_puts etc.  Should only happen for
    // user space programs built into the kernel; ordinary programs
    // won't have this definition of panic accessible.

    syscall3(write, 0, "PANIC: ", 7);
    syscall3(write, 0, explanation, strlen(explanation));
    syscall3(write, 0, "\n", 1);

    if (proc_current_pid()) {
      syscall1(exit, 14);
    }
  }
  else {
    if (proc_current_pid()) {
      // This might be an interrupt from a user space process, but it
      // might also be something else.
      // TODO: don't kill the current process if this error doesn't
      // actually come from a process
      dbg_puts(sys, "panic: %s", explanation);
      kill_caller();
    }
    else {
      // Before we've started executing processes
      print("PANIC! ");
      print(explanation);
    }
  }

  while (true) {
    asm volatile("cli");
    // NB: not HLTing; bochs freezes then
  }
}

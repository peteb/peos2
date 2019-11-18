#include "syscalls.h"
#include "x86.h"
#include "protected_mode.h"
#include "screen.h"
#include "debug.h"
#include "syscall_utils.h"

#include "support/format.h"
#include "support/utils.h"

#include <stdint.h>

extern "C" void int_syscall(volatile isr_registers *);
extern "C" void isr_syscall(isr_registers *);

static void *syscalls[0xFF];

void syscalls_init()
{
  int_register(0x90, isr_syscall, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
}

extern "C" void int_syscall(volatile isr_registers *regs)
{
  uint32_t syscall_num = regs->eax;

  if (syscall_num >= ARRAY_SIZE(syscalls)) {
    // TODO: should we kill the process?
    dbg_puts(syscall, "process invoked invalid syscall %d", syscall_num);
    kill_caller();
  }

  syscall_fun handler = (syscall_fun)syscalls[syscall_num];
  assert(handler);
  regs->eax = handler(regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi, regs);
}

void syscall_register(int num, syscall_fun handler)
{
  syscalls[num] = (void *)handler;
}

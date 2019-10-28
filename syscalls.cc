#include "syscalls.h"
#include "x86.h"
#include "support/format.h"
#include "protected_mode.h"
#include "screen.h"

#include <stdint.h>

extern "C" void int_syscall(volatile isr_registers);
static uint32_t syscall_puts(char *);

extern "C" void isr_syscall(isr_registers);

static void *syscalls[0xFF] = {
  (void *)&syscall_puts
};

void syscalls_init() {
  int_register(0x90, isr_syscall, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
}

extern "C" void int_syscall(volatile isr_registers regs) {
  uint32_t syscall_num = regs.eax;

  if (syscall_num >= sizeof(syscalls) / sizeof(syscalls[0])) {
    // TODO: should we kill the process?
    panic("Called invalid syscall");
  }

  syscall_fun handler = (syscall_fun)syscalls[syscall_num];
  assert(handler);
  regs.eax = handler(regs.ebx, regs.ecx, regs.edx, regs.esi, regs.edi);
}

void syscall_register(int num, syscall_fun handler) {
  syscalls[num] = (void *)handler;
}

uint32_t syscall_puts(char *msg) {
  print("SYSCALL: ");
  puts(msg);
  return 0xBEEF;
}

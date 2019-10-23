#include "syscalls.h"
#include "x86.h"
#include "support/format.h"
#include "protected_mode.h"
#include "screen.h"

extern "C" void int_syscall(volatile isr_registers);
static uint32_t syscall_puts(char *);

extern "C" void isr_syscall(isr_registers);

static void *syscalls[] = {
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

  uint32_t ret;
  asm volatile("push %1\n"
               "push %2\n"
               "push %3\n"
               "push %4\n"
               "push %5\n"
               "call %6\n"
               "add esp, 20\n"
               :
               "=a" (ret)
               :
               "r" (regs.edi),
               "r" (regs.esi),
               "r" (regs.edx),
               "r" (regs.ecx),
               "r" (regs.ebx),
               "r" (syscalls[syscall_num]));
  regs.eax = ret;  // We can return eax by writing it into the stack (thanks volatile!)
}

uint32_t syscall_puts(char *msg) {
  print("SYSCALL: ");
  puts(msg);
  return 0xBEEF;
}

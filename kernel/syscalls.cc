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
static int syscall_strerror(int code, char *buf, int len);

static void *syscalls[SYSCALL_NUM_MAX];

void syscalls_init()
{
  int_register(0x90, isr_syscall, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  syscall_register(SYSCALL_NUM_STRERROR, (syscall_fun)syscall_strerror);
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
  // TODO: change num to unsigned
  assert(num < (int)ARRAY_SIZE(syscalls));
  syscalls[num] = (void *)handler;
}

static int syscall_strerror(int code, char *buf, int len)
{
  verify_ptr(sys, buf);
  // TODO: verify buf + len area
  const char *message = nullptr;

  switch (code) {
  case ENOSUPPORT:
    message = "Operation not supported";
    break;

  case ENOENT:
    message = "Entry missing";
    break;

  case ENODIR:
    message = "Path parent isn't a directory";
    break;

  case ENODRIVER:
    message = "Path doesn't contain a mapped driver";
    break;

  case ENOSPACE:
    message = "No space in structure or file";
    break;

  case EINCONSTATE:
    message = "Internal state is inconsistent";
    break;

  case EINVVAL:
    message = "Invalid value";
    break;

  default:
    return -1;
  }

  strncpy(buf, message, len);

  return 0;
}

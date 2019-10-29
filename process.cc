#include "process.h"
#include "protected_mode.h"
#include "screen.h"
#include "assert.h"
#include "syscalls.h"

#include "support/pool.h"
#include "support/format.h"

class process_control_block {
public:
  process_control_block(uint32_t *kernel_stack, uint32_t *user_stack)
    : kernel_esp(kernel_stack),
      kernel_stack(kernel_stack),
      user_esp(user_stack)
  {
  }

  process_control_block(const process_control_block &other) {
    *this = other;
  }

  // TODO: delete these below
  process_control_block() = default;

  process_control_block &operator =(const process_control_block &other) {
    this->kernel_esp = other.kernel_esp;
    this->user_esp = other.user_esp;
    return *this;
  }

  void kpush(uint32_t value) {
    *--kernel_esp = value;
  }

  void activate_kernel_stack() {
    tss_set_kernel_stack((uint32_t)kernel_stack);
  }

  uint32_t *kernel_esp, *kernel_stack, *user_esp;
};

//
// Wrapper for a kernel or user stack.
//
template<size_t N>
struct stack {
  uint32_t *bottom_of_stack() {
    size_t length = sizeof(data) / sizeof(data[0]);
    assert(length > 0);
    return &data[length - 1];
  }

  // Stacks need to be aligned on 16 byte boundaries
  uint32_t data[N] alignas(16);
};

extern "C" void switch_task(uint32_t *old_esp, uint32_t new_esp);
extern "C" void switch_task_iret();
static uint32_t syscall_yield();

static p2::pool<process_control_block, 16, proc_handle> processes;
static p2::pool<stack<1024>, 16> user_stacks;
static p2::pool<stack<256>, 16> kernel_stacks;
static process_control_block *current_task = nullptr;

static uint32_t *alloc_kernel_stack() {
  uint16_t id = kernel_stacks.emplace_back();
  return kernel_stacks[id].bottom_of_stack();
}

static uint32_t *alloc_user_stack() {
  uint16_t id = user_stacks.emplace_back();
  return user_stacks[id].bottom_of_stack();
}

void proc_init() {
  syscall_register(SYSCALL_NUM_YIELD, (syscall_fun)syscall_yield);
}

proc_handle proc_create(void *eip) {
  proc_handle pid = processes.emplace_back();
  process_control_block &pcb = processes[pid];

  pcb.kernel_stack = alloc_kernel_stack();
  pcb.user_esp = alloc_user_stack();
  pcb.kernel_esp = pcb.kernel_stack;

  // We need a stack that can invoke iret as soon as possible, without
  // invoking any gcc function epilogues or prologues.  First, we need
  // a stack good for `switch_task`. As we set the return address to
  // be `switch_task_iret`, we also need to push values for IRET.
  pcb.kpush(USER_DATA_SEL);              // SS
  pcb.kpush((uint32_t)pcb.user_esp); // ESP
  pcb.kpush(0x202);                      // EFLAGS, IF
  pcb.kpush(USER_CODE_SEL);              // CS
  pcb.kpush((uint32_t)eip);              // Return EIP from switch_task_iret
  pcb.kpush((uint32_t)switch_task_iret); // Return EIP from switch_task
  pcb.kpush(0);                          // EBX
  pcb.kpush(0);                          // ESI
  pcb.kpush(0);                          // EDI
  pcb.kpush(0);                          // EBP

  return pid;
}

void proc_switch(proc_handle pid) {
  // Receiving an interrupt between updating TSS.ESP0 and IRET is not
  // something we want, so disable interrupts.
  asm volatile("cli");

  process_control_block &pcb = processes[pid];
  pcb.activate_kernel_stack();

  uint32_t *current_task_esp = 0;
  uint32_t **current_task_esp_ptr = &current_task_esp;

  if (current_task == &pcb) {
    return;
  }

  if (current_task) {
    current_task_esp_ptr = &current_task->kernel_esp;
  }

  current_task = &pcb;
  switch_task((uint32_t *)current_task_esp_ptr, (uint32_t)pcb.kernel_esp);
}

uint32_t syscall_yield() {
  static proc_handle pid = 0;
  proc_switch(pid++ % processes.size());
  // TODO: remove above. It doesn't work when a process has been removed
  return 0;
}

#include "process.h"
#include "protected_mode.h"
#include "screen.h"
#include "assert.h"
#include "syscalls.h"
#include "x86.h"

#include "support/pool.h"
#include "support/format.h"
#include "support/limits.h"

class process_control_block {
public:
  process_control_block(uint32_t *kernel_stack, uint32_t *user_stack)
    : kernel_esp(kernel_stack),
      kernel_stack(kernel_stack),
      user_esp(user_stack),
      suspended(false),
      last_tick(0)
  {}

  process_control_block(const process_control_block &other) = delete;
  process_control_block &operator =(const process_control_block &other) = delete;

  void kpush(uint32_t value) {
    *--kernel_esp = value;
  }

  void activate_kernel_stack() {
    tss_set_kernel_stack((uint32_t)kernel_stack);
  }

  uint32_t *kernel_esp;
  uint32_t *kernel_stack;
  uint32_t *user_esp;
  bool suspended;
  proc_handle prev_process, next_process;
  uint64_t last_tick;
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
    // You can argue that &data[length] would be correct, ie, one
    // above the end of the array: it's explicitly a legal pointer in
    // C/C++, and x86 PUSH first decrements ESP before writing to
    // [ESP].
  }

  // Stacks need to be aligned on 16 byte boundaries
  uint32_t data[N] alignas(16);
};

// Externals
extern "C" void switch_task(uint32_t *old_esp, uint32_t new_esp);
extern "C" void switch_task_iret();
extern "C" void isr_timer(isr_registers);

// Statics
static uint32_t syscall_yield();
static uint32_t *alloc_kernel_stack();
static uint32_t *alloc_user_stack();
static proc_handle decide_next_process();
static void enqueue_front(proc_handle pid, proc_handle *head);
static void dequeue(proc_handle pid, proc_handle *head);
static proc_handle create_process(void *eip);
static void idle_main();

// Global state
static p2::pool<process_control_block, 16, proc_handle> processes;
static p2::pool<stack<1024>, 16> user_stacks;
static p2::pool<stack<256>, 16> kernel_stacks;
static process_control_block *current_task;
static proc_handle running_head = processes.end(), suspended_head = processes.end();
static proc_handle idle_process = processes.end();

static uint64_t tick_count;

void proc_init() {
  // Syscalls
  syscall_register(SYSCALL_NUM_YIELD, (syscall_fun)syscall_yield);

  // Timer for preemptive task switching
  pit_set_phase(10);
  irq_enable(IRQ_SYSTEM_TIMER);
  int_register(IRQ_BASE_INTERRUPT + IRQ_SYSTEM_TIMER, isr_timer, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P);

  idle_process = create_process((void *)idle_main);
}

proc_handle proc_create(void *eip) {
  proc_handle pid = create_process(eip);
  enqueue_front(pid, &running_head);

  return pid;
}

extern "C" void int_timer(isr_registers) {
  ++tick_count;
  irq_eoi(IRQ_SYSTEM_TIMER);
  proc_yield();
}

void proc_switch(proc_handle pid) {
  // Receiving an interrupt between updating TSS.ESP0 and IRET is not
  // something we want, so disable interrupts.
  asm volatile("cli");

  process_control_block &pcb = processes[pid];
  pcb.activate_kernel_stack();
  pcb.last_tick = tick_count;

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

void proc_yield() {
  proc_switch(decide_next_process());
}

void proc_suspend(proc_handle pid) {
  process_control_block &pcb = processes[pid];
  if (pcb.suspended) {
    return;
  }

  dequeue(pid, &running_head);
  enqueue_front(pid, &suspended_head);
}

void proc_resume(proc_handle pid) {
  process_control_block &pcb = processes[pid];
  if (!pcb.suspended) {
    return;
  }

  dequeue(pid, &suspended_head);
  enqueue_front(pid, &running_head);
}

static void enqueue_front(proc_handle pid, proc_handle *head) {
  // TODO: extract linked list logic
  if (*head != processes.end()) {
    processes[*head].prev_process = pid;
  }

  process_control_block &pcb = processes[pid];
  pcb.next_process = *head;
  pcb.prev_process = processes.end();
  *head = pid;
}

static void dequeue(proc_handle pid, proc_handle *head) {
  process_control_block &pcb = processes[pid];
  if (pcb.prev_process != processes.end()) {
    processes[pcb.prev_process].next_process = pcb.next_process;
  }

  if (pcb.next_process != processes.end()) {
    processes[pcb.next_process].prev_process = pcb.prev_process;
  }

  if (*head == pid) {
    *head = pcb.next_process;
  }
}

static uint32_t *alloc_kernel_stack() {
  uint16_t id = kernel_stacks.emplace_back();
  return kernel_stacks[id].bottom_of_stack();
}

static uint32_t *alloc_user_stack() {
  uint16_t id = user_stacks.emplace_back();
  return user_stacks[id].bottom_of_stack();
}

static proc_handle decide_next_process() {
  // TODO: this algorithm has a bias towards the front of the list due
  // to the < comparison. Make it more fair.
  uint64_t minimum_tick = p2::numeric_limits<uint64_t>::max();
  proc_handle minimum_pid = processes.end();
  proc_handle node = running_head;

  while (node != processes.end()) {
    process_control_block &pcb = processes[node];
    if (pcb.last_tick < minimum_tick) {
      minimum_tick = pcb.last_tick;
      minimum_pid = node;
    }

    node = pcb.next_process;
  }

  if (minimum_pid == processes.end()) {
    return idle_process;
  }

  return minimum_pid;
}

static proc_handle create_process(void *eip) {
  proc_handle pid = processes.emplace_back(alloc_kernel_stack(), alloc_user_stack());
  process_control_block &pcb = processes[pid];

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

static uint32_t syscall_yield() {
  proc_yield();
  return 0;
}

static void idle_main() {
  static int count = 0;
  while (true) {
    *(volatile char *)0xB8000 = 'A' + count;
    count = (count + 1) % 26;
  }
}

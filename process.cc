#include "process.h"
#include "protected_mode.h"
#include "screen.h"
#include "assert.h"
#include "syscalls.h"
#include "x86.h"
#include "memareas.h"
#include "memory.h"

#include "support/pool.h"
#include "support/format.h"
#include "support/limits.h"

SYSCALL_DEF1(exit, SYSCALL_NUM_EXIT, uint32_t);

class process_control_block {
public:
  process_control_block(uint32_t *kernel_stack, uint32_t *user_stack, mem_adrspc address_space)
    : kernel_esp(kernel_stack),
      kernel_stack(kernel_stack),
      user_esp(user_stack),
      address_space(address_space),
      suspended(false),
      terminating(false),
      last_tick(0)
  {
    argv[0] = argument.c_str();
  }

  process_control_block(const process_control_block &other) = delete;
  process_control_block &operator =(const process_control_block &other) = delete;

  void kpush(uint32_t value) {
    *--kernel_esp = value;
  }

  void upush(uint32_t value) {
    *--user_esp = value;
  }

  void activate() {
    tss_set_kernel_stack((uint32_t)kernel_stack);
    mem_activate_address_space(address_space);
  }

  uint32_t *kernel_esp;
  uint32_t *kernel_stack;
  uint32_t *user_esp;
  uint16_t kernel_stack_idx, user_stack_idx;

  const char *argv[1];
  p2::string<64> argument;

  mem_adrspc address_space;

  bool suspended, terminating;
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
static uint32_t    syscall_yield();
static uint32_t    syscall_exit(uint32_t exit_code);
static uint32_t    syscall_kill(uint32_t pid);
static proc_handle decide_next_process();
static proc_handle create_process(void *eip, const char *argument);
static void        destroy_process(proc_handle pid);
static void        switch_process(proc_handle pid);

static void        enqueue_front(proc_handle pid, proc_handle *head);
static void        dequeue(proc_handle pid, proc_handle *head);

static void        idle_main();
static void        _user_proc_cleanup();

// Global state
static p2::pool<process_control_block, 16, proc_handle> processes;
static p2::pool<stack<1024>, 16> user_stacks;
static p2::pool<stack<256>, 16> kernel_stacks;

static proc_handle current_pid = processes.end();
static proc_handle running_head = processes.end(), suspended_head = processes.end();
static proc_handle idle_process = processes.end();

static uint64_t tick_count;

void proc_init() {
  // Syscalls
  syscall_register(SYSCALL_NUM_YIELD, (syscall_fun)syscall_yield);
  syscall_register(SYSCALL_NUM_EXIT, (syscall_fun)syscall_exit);
  syscall_register(SYSCALL_NUM_KILL, (syscall_fun)syscall_kill);

  // Timer for preemptive task switching
  pit_set_phase(10);
  irq_enable(IRQ_SYSTEM_TIMER);
  int_register(IRQ_BASE_INTERRUPT + IRQ_SYSTEM_TIMER, isr_timer, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P);

  idle_process = create_process((void *)idle_main, "");
}

proc_handle proc_create(void *eip, const char *argument) {
  // TODO: support multiple arguments
  proc_handle pid = create_process(eip, argument);
  enqueue_front(pid, &running_head);
  return pid;
}

static void destroy_process(proc_handle pid) {
  process_control_block &pcb = processes[pid];

  // Free up memory used by the process
  user_stacks.erase(pcb.user_stack_idx);
  kernel_stacks.erase(pcb.kernel_stack_idx);
  pcb.kernel_esp = pcb.kernel_stack = pcb.user_esp = nullptr;
  mem_destroy_address_space(pcb.address_space);

  // Remove the process so it won't get picked for execution
  dequeue(pid, pcb.suspended ? &suspended_head : &running_head);

  // TODO: we might want to keep the PCB around for a while so we can
  // read the exit status, detect dangling PIDs, etc...
  processes.erase(pid);
}


proc_handle proc_current_pid() {
  return current_pid;
}

extern "C" void int_timer(isr_registers) {
  ++tick_count;
  irq_eoi(IRQ_SYSTEM_TIMER);
  proc_yield();
}

void proc_switch(proc_handle pid) {
  process_control_block &pcb = processes[pid];
  assert(!pcb.suspended && "please resume the process before switching to it");

  if (pcb.terminating) {
    puts("proc: trying to switch to terminating process");
    // The process has been terminated, so we cannot run it. This can
    // happen if the process was in a blocking syscall when someone
    // else asked to kill it.
    destroy_process(pid);
    pid = decide_next_process();
  }

  switch_process(pid);
}

static void switch_process(proc_handle pid) {
  // Receiving an interrupt between updating TSS.ESP0 and IRET is not
  // something we want, so disable interrupts.
  asm volatile("cli");

  process_control_block &pcb = processes[pid];
  pcb.activate();
  pcb.last_tick = tick_count;

  if (current_pid == pid) {
    return;
  }

  uint32_t *current_task_esp = 0;
  uint32_t **current_task_esp_ptr = &current_task_esp;

  if (current_pid != processes.end()) {
    current_task_esp_ptr = &processes[current_pid].kernel_esp;
  }

  current_pid = pid;
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
  pcb.suspended = true;
}

void proc_resume(proc_handle pid) {
  process_control_block &pcb = processes[pid];
  if (!pcb.suspended) {
    return;
  }

  dequeue(pid, &suspended_head);
  enqueue_front(pid, &running_head);
  pcb.suspended = false;
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

static proc_handle create_process(void *eip, const char *argument) {
  uint16_t ks_idx = kernel_stacks.emplace_back();
  uint16_t us_idx = user_stacks.emplace_back();

  proc_handle pid = processes.emplace_back(kernel_stacks[ks_idx].bottom_of_stack(),
                                           user_stacks[us_idx].bottom_of_stack(),
                                           mem_create_address_space());
  process_control_block &pcb = processes[pid];
  pcb.kernel_stack_idx = ks_idx;
  pcb.user_stack_idx = us_idx;
  pcb.argument = argument;

  // Push user space stack things first: the kernel stack references
  // the user ESP.
  pcb.upush((uint32_t)pcb.argv);         // argv
  pcb.upush(1);                          // argc
  pcb.upush((uint32_t)_user_proc_cleanup);

  // We need a stack that can invoke iret as soon as possible, without
  // invoking any gcc function epilogues or prologues.  First, we need
  // a stack good for `switch_task`. As we set the return address to
  // be `switch_task_iret`, we also need to push values for IRET.
  pcb.kpush(USER_DATA_SEL);              // SS
  pcb.kpush((uint32_t)pcb.user_esp);     // ESP
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

void proc_kill(proc_handle pid, uint32_t exit_status) {
  process_control_block &pcb = processes[pid];

  if (pcb.suspended) {
    // If the process is suspended we need to wait for the driver
    puts(p2::format<64>("pid %d exiting with status %d") % pid % exit_status);
    pcb.terminating = true;
    return;
  }

  puts(p2::format<64>("pid %d exited with status %d") % pid % exit_status);
  destroy_process(pid);
}

static uint32_t syscall_exit(uint32_t exit_status) {
  proc_kill(proc_current_pid(), exit_status);
  proc_yield();
  assert(false && "unreachable");
  // We can't return from this one; there's nothing left to do in the process
  return 0;
}

static uint32_t syscall_kill(uint32_t pid) {
  proc_kill(pid, 123);
  return 1;
}

//
// Idling process: when there's nothing else to do.
//
static void idle_main() {
  static int count = 0;
  while (true) {
    *(volatile char *)PHYS2KERVIRT(0xB8000) = 'A' + count;
    count = (count + 1) % 26;
    __builtin_ia32_pause();
  }
}

//
// Called in the user process when the main function has
// returned. Should be generated by the user space compiler/linker in
// the future, but we don't have any memory protection now so a
// runaway process will crash the whole system.
//
static void _user_proc_cleanup() {
  uint32_t retval;
  asm volatile("" : "=a"(retval));
  SYSCALL1(exit, retval);
}

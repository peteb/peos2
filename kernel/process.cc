#include "process.h"
#include "protected_mode.h"
#include "screen.h"
#include "assert.h"
#include "syscalls.h"
#include "x86.h"
#include "memareas.h"
#include "memory.h"
#include "filesystem.h"
#include "debug.h"
#include "elf.h"

#include "support/pool.h"
#include "support/format.h"
#include "support/limits.h"

#include "process_private.h"

SYSCALL_DEF1(exit, SYSCALL_NUM_EXIT, uint32_t);

// Externals
extern "C" void switch_task(uint32_t *old_esp, uint32_t new_esp);
extern "C" void switch_task_iret();
extern "C" void isr_timer(isr_registers);
extern "C" void _user_proc_cleanup();

// Statics
static uint32_t    syscall_yield();
static uint32_t    syscall_exit(int exit_code);
static uint32_t    syscall_kill(uint32_t pid);
static int         syscall_spawn(const char *filename);
static proc_handle decide_next_process();
static void        destroy_process(proc_handle pid);
static void        switch_process(proc_handle pid);

static void        enqueue_front(proc_handle pid, proc_handle *head);
static void        dequeue(proc_handle pid, proc_handle *head);

static void        idle_main();

// Global state
static p2::pool<process_control_block, 16, proc_handle> processes;
static p2::pool<stack<0x1000>, 16> user_stacks;
static p2::pool<stack<256>, 16> kernel_stacks;

static proc_handle current_pid = processes.end();
static proc_handle running_head = processes.end(), suspended_head = processes.end();
static proc_handle idle_process = processes.end();

static uint64_t tick_count;

// Definitions
void proc_init()
{
  // Syscalls
  syscall_register(SYSCALL_NUM_YIELD, (syscall_fun)syscall_yield);
  syscall_register(SYSCALL_NUM_EXIT, (syscall_fun)syscall_exit);
  syscall_register(SYSCALL_NUM_KILL, (syscall_fun)syscall_kill);
  syscall_register(SYSCALL_NUM_SPAWN, (syscall_fun)syscall_spawn);

  // Timer for preemptive task switching
  pit_set_phase(10);
  irq_enable(IRQ_SYSTEM_TIMER);
  int_register(IRQ_BASE_INTERRUPT + IRQ_SYSTEM_TIMER, isr_timer, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P);

  idle_process = proc_create(PROC_USER_SPACE|PROC_KERNEL_ACCESSIBLE, "");
  proc_setup_stack(idle_process, (void *)idle_main);
}

proc_handle proc_create(uint32_t flags, const char *argument)
{
  // TODO: support multiple arguments
  mem_space space_handle = mem_create_space();
  uint16_t mapping_flags = MEM_PE_P|MEM_PE_RW;
  // We must always map the kernel as RW; it's going to be used in
  // syscalls and other interrupts even if the U bit isn't set.
  assert(flags & PROC_USER_SPACE);

  if (flags & PROC_KERNEL_ACCESSIBLE) {
    mapping_flags |= MEM_PE_U;
  }

  mem_map_kernel(space_handle, mapping_flags);

  proc_handle pid = processes.emplace_back(space_handle);
  process_control_block &pcb = processes[pid];
  pcb.argument = argument;

  return pid;
}

void proc_setup_stack(proc_handle pid, void *eip)
{
  process_control_block &pcb = processes[pid];

  uint16_t kernel_stack_handle = kernel_stacks.emplace_back();
  uint16_t user_stack_handle = user_stacks.emplace_back();
  pcb.set_kernel_stack(kernel_stacks[kernel_stack_handle], kernel_stack_handle);
  pcb.set_user_stack(user_stacks[user_stack_handle], user_stack_handle);

  // TODO: support non-user space processes

  // Push user space stack things first: the kernel stack references
  // the user ESP.
  uint32_t *initial_user_esp = pcb.user_esp;
  pcb.upush((uintptr_t)pcb.argv);        // argv
  pcb.upush(1);                          // argc
  pcb.upush((uintptr_t)_user_proc_cleanup);

  size_t initial_stack_usage = (uintptr_t)initial_user_esp - (uintptr_t)pcb.user_esp;
  uintptr_t user_space_stack_base = 0xB0000000;

  // We need a stack that can invoke iret as soon as possible, without
  // invoking any gcc function epilogues or prologues.  First, we need
  // a stack good for `switch_task`. As we set the return address to
  // be `switch_task_iret`, we also need to push values for IRET.
  pcb.kpush(USER_DATA_SEL);                                // SS
  pcb.kpush(user_space_stack_base - initial_stack_usage);  // ESP
  pcb.kpush(0x202);                                        // EFLAGS, IF
  pcb.kpush(USER_CODE_SEL);                                // CS
  pcb.kpush((uint32_t)eip);                                // Return EIP from switch_task_iret
  pcb.kpush((uint32_t)switch_task_iret);                   // Return EIP from switch_task
  pcb.kpush(0);                                            // EBX
  pcb.kpush(0);                                            // ESI
  pcb.kpush(0);                                            // EDI
  pcb.kpush(0);                                            // EBP

  size_t stack_area_size = 4096 * 1024;
  size_t guard_size = 4096 * 10;

  // The first page of the stack is in a kernel pool, this is an easy
  // way for the kernel to be able to populate the stack, albeit
  // wasteful. The rest of the stack pages will be allocated on the
  // fly.
  mem_space space_handle = proc_get_space(pid);
  mem_map_linear(space_handle, user_space_stack_base - 0x1000, user_space_stack_base, KERNVIRT2PHYS((uintptr_t)user_stacks[user_stack_handle].base()) - 0x1000, MEM_PE_P|MEM_PE_RW|MEM_PE_U);
  mem_map_alloc(space_handle, user_space_stack_base - stack_area_size, user_space_stack_base - 0x1000, MEM_PE_P|MEM_PE_RW|MEM_PE_U);
  mem_map_guard(space_handle, user_space_stack_base - stack_area_size - guard_size, user_space_stack_base - stack_area_size);
}

void proc_enqueue(proc_handle pid)
{
  // TODO: check status and that it's not on a queue already
  enqueue_front(pid, &running_head);
}

static void destroy_process(proc_handle pid)
{
  process_control_block &pcb = processes[pid];

  // TODO: performance: we're wasting time iterating over non-valid
  // elements
  for (size_t i = 0; i < pcb.file_descriptors.watermark(); ++i) {
    if (!pcb.file_descriptors.valid(i)) {
      continue;
    }

    dbg_puts(proc, "forcefully closing fd %d", i);
    vfs_close_handle(pid, i);
  }

  mem_destroy_space(pcb.space_handle);

  // Remove the process so it won't get picked for execution
  dequeue(pid, pcb.suspended ? &suspended_head : &running_head);

  // Free up memory used by the process
  user_stacks.erase(pcb.user_stack_handle);
  kernel_stacks.erase(pcb.kernel_stack_handle);
  pcb.kernel_esp = pcb.kernel_stack_base = pcb.user_esp = nullptr;

  // TODO: we might want to keep the PCB around for a while so we can
  // read the exit status, detect dangling PIDs, etc...
  processes.erase(pid);
}


proc_handle proc_current_pid()
{
  return current_pid;
}

extern "C" void int_timer(isr_registers)
{
  ++tick_count;
  irq_eoi(IRQ_SYSTEM_TIMER);
  proc_yield();
}

void proc_switch(proc_handle pid)
{
  process_control_block &pcb = processes[pid];
  assert(!pcb.suspended && "please resume the process before switching to it");

  if (pcb.terminating) {
    dbg_puts(proc, "trying to switch to terminating process %d", pid);
    // The process has been terminated, so we cannot run it. This can
    // happen if the process was in a blocking syscall when someone
    // else asked to kill it.
    destroy_process(pid);
    pid = decide_next_process();
  }

  switch_process(pid);
}

static void switch_process(proc_handle pid)
{
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

  // Save the old process' kernel stack pointer. But only if the old
  // process still exists.
  if (current_pid != processes.end() && processes.valid(current_pid)) {
    current_task_esp_ptr = &processes[current_pid].kernel_esp;
  }

  current_pid = pid;
  switch_task((uint32_t *)current_task_esp_ptr, (uint32_t)pcb.kernel_esp);
}

void proc_yield()
{
  proc_switch(decide_next_process());
}

void proc_suspend(proc_handle pid)
{
  process_control_block &pcb = processes[pid];
  if (pcb.suspended) {
    return;
  }

  dequeue(pid, &running_head);
  enqueue_front(pid, &suspended_head);
  pcb.suspended = true;
}

void proc_resume(proc_handle pid)
{
  process_control_block &pcb = processes[pid];
  if (!pcb.suspended) {
    return;
  }

  dequeue(pid, &suspended_head);
  enqueue_front(pid, &running_head);
  pcb.suspended = false;
}

static void enqueue_front(proc_handle pid, proc_handle *head)
{
  // TODO: extract linked list logic
  if (*head != processes.end()) {
    processes[*head].prev_process = pid;
  }

  process_control_block &pcb = processes[pid];
  pcb.next_process = *head;
  pcb.prev_process = processes.end();
  *head = pid;
}

static void dequeue(proc_handle pid, proc_handle *head)
{
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

static proc_handle decide_next_process()
{
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

static uint32_t syscall_yield()
{
  proc_yield();
  return 0;
}

void proc_kill(proc_handle pid, uint32_t exit_status)
{
  process_control_block &pcb = processes[pid];

  if (pcb.suspended) {
    // If the process is suspended we need to wait for the driver
    dbg_puts(proc, "pid %d exiting with status %d", pid, exit_status);
    pcb.terminating = true;
    return;
  }

  dbg_puts(proc, "pid %d exited with status %d", pid, exit_status);
  destroy_process(pid);
}

int proc_create_fd(proc_handle pid, proc_fd fd)
{
  process_control_block &pcb = processes[pid];
  return pcb.file_descriptors.push_back(fd);
}

proc_fd *proc_get_fd(proc_handle pid, int fd)
{
  return &processes[pid].file_descriptors[fd];
}

void proc_remove_fd(proc_handle pid, int fd)
{
  processes[pid].file_descriptors.erase(fd);
}

mem_space proc_get_space(proc_handle pid)
{
  return processes[pid].space_handle;
}

static uint32_t syscall_exit(int exit_status)
{
  proc_kill(proc_current_pid(), exit_status);
  proc_yield();
  assert(false && "unreachable");
  // We can't return from this one; there's nothing left to do in the process
  return 0;
}

static uint32_t syscall_kill(uint32_t pid)
{
  proc_kill(pid, 123);
  return 1;
}

static int syscall_spawn(const char *filename)
{
  dbg_puts(proc, "spawning process with image '%s'", filename);
  // TODO: handle flags
  proc_handle pid = proc_create(PROC_USER_SPACE|PROC_KERNEL_ACCESSIBLE, "");
  elf_map_process(pid, filename);
  proc_enqueue(pid);
  return pid;
}

//
// Idling process: when there's nothing else to do.
//
static void idle_main()
{
  static int count = 0;
  while (true) {
    *(volatile char *)PHYS2KERNVIRT(0xB8000) = 'A' + count;
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
extern "C" void _user_proc_cleanup()
{
  uint32_t retval;
  asm volatile("" : "=a"(retval));
  SYSCALL1(exit, retval);
}
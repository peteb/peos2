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
#include "syscall_utils.h"

#include "support/pool.h"
#include "support/format.h"
#include "support/limits.h"

#include "process_private.h"

SYSCALL_DEF1(exit, SYSCALL_NUM_EXIT, uint32_t);

// Externals
extern "C" void isr_timer(isr_registers);

// Statics
static uint32_t    syscall_yield();
static uint32_t    syscall_exit(int exit_code);
static uint32_t    syscall_kill(uint32_t pid);
static int         syscall_spawn(const char *filename);
static int         syscall_exec(const char *filename, const char **argv);

static proc_handle decide_next_process();
static void        destroy_process(proc_handle pid);
static void        switch_process(proc_handle pid);

static void        enqueue_front(proc_handle pid, proc_handle *head);
static void        dequeue(proc_handle pid, proc_handle *head);

static void        idle_main();

// Global state
static p2::pool<process, 16, proc_handle> processes;

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
  syscall_register(SYSCALL_NUM_EXEC, (syscall_fun)syscall_exec);

  // Timer for preemptive task switching
  pit_set_phase(10);
  irq_enable(IRQ_SYSTEM_TIMER);
  int_register(IRQ_BASE_INTERRUPT + IRQ_SYSTEM_TIMER, isr_timer, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P);

  idle_process = proc_create(PROC_USER_SPACE|PROC_KERNEL_ACCESSIBLE, "");
  proc_set_syscall_ret(idle_process, (uintptr_t)idle_main);
}

proc_handle proc_create(uint32_t flags, const char *argument)
{
  // TODO: support multiple arguments
  mem_space space_handle = mem_create_space();
  uint16_t mapping_flags = MEM_AREA_READWRITE;

  // We must always map the kernel as RW; it's going to be used in
  // syscalls and other interrupts even if the U bit isn't set.
  assert(flags & PROC_USER_SPACE);

  if (flags & PROC_KERNEL_ACCESSIBLE) {
    mapping_flags |= (MEM_AREA_USER|MEM_AREA_SYSCALL);
  }

  mem_map_kernel(space_handle, mapping_flags|MEM_AREA_RETAIN_EXEC);

  proc_handle pid = processes.emplace_back(space_handle,
                                           *vfs_create_context(),
                                           flags);
  processes[pid].setup_stacks();

  const char *args[] = {argument};
  processes[pid].assign_user_stack(args, 1);

  return pid;
}

void proc_assign_user_stack(proc_handle pid, const char *argv[], int argc)
{
  processes[pid].assign_user_stack(argv, argc);
}

void proc_set_syscall_ret(proc_handle pid, uintptr_t ip)
{
  processes[pid].set_syscall_ret_ip(ip);
}

void proc_enqueue(proc_handle pid)
{
  // TODO: check status and that it's not on a queue already
  enqueue_front(pid, &running_head);
}

static void destroy_process(proc_handle pid)
{
  process &proc = processes[pid];
  vfs_destroy_context(proc.file_context);
  mem_destroy_space(proc.space_handle);

  // Remove the process so it won't get picked for execution
  dequeue(pid, proc.suspended ? &suspended_head : &running_head);

  proc.free_stacks();

  // TODO: we might want to keep the PCB around for a while so we can
  // read the exit status, detect dangling PIDs, etc...
  processes.erase(pid);
}


p2::opt<proc_handle> proc_current_pid()
{
  if (current_pid != processes.end())
    return current_pid;

  return {};
}

extern "C" void int_timer(isr_registers)
{
  ++tick_count;
  irq_eoi(IRQ_SYSTEM_TIMER);
  proc_yield();
}

void proc_switch(proc_handle pid)
{
  process &proc = processes[pid];
  assert(!proc.suspended && "please resume the process before switching to it");

  if (proc.terminating) {
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

  process &proc = processes[pid];

  // Update last_tick so the scheduler will work correctly
  proc.last_tick = tick_count;

  if (current_pid == pid) {
    return;
  }

  process *previous_proc = processes.valid(current_pid) ? &processes[current_pid] : nullptr;
  current_pid = pid;
  proc.activate(previous_proc);
}

void proc_yield()
{
  proc_switch(decide_next_process());
}

void proc_suspend(proc_handle pid)
{
  process &proc = processes[pid];
  if (proc.suspended) {
    return;
  }

  dequeue(pid, &running_head);
  enqueue_front(pid, &suspended_head);
  proc.suspended = true;
}

void proc_resume(proc_handle pid)
{
  process &proc = processes[pid];
  if (!proc.suspended) {
    return;
  }

  dequeue(pid, &suspended_head);
  enqueue_front(pid, &running_head);
  proc.suspended = false;
}

static void enqueue_front(proc_handle pid, proc_handle *head)
{
  // TODO: extract linked list logic
  if (*head != processes.end()) {
    processes[*head].prev_process = pid;
  }

  process &proc = processes[pid];
  proc.next_process = *head;
  proc.prev_process = processes.end();
  *head = pid;
}

static void dequeue(proc_handle pid, proc_handle *head)
{
  process &proc = processes[pid];
  if (proc.prev_process != processes.end()) {
    processes[proc.prev_process].next_process = proc.next_process;
  }

  if (proc.next_process != processes.end()) {
    processes[proc.next_process].prev_process = proc.prev_process;
  }

  if (*head == pid) {
    *head = proc.next_process;
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
    process &proc = processes[node];
    if (proc.last_tick < minimum_tick) {
      minimum_tick = proc.last_tick;
      minimum_pid = node;
    }

    node = proc.next_process;
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
  process &proc = processes[pid];

  if (proc.suspended) {
    // If the process is suspended we need to wait for the driver
    dbg_puts(proc, "pid %d exiting with status %d", pid, exit_status);
    proc.terminating = true;
    return;
  }

  dbg_puts(proc, "pid %d exited with status %d", pid, exit_status);
  destroy_process(pid);
}

mem_space proc_get_space(proc_handle pid)
{
  return processes[pid].space_handle;
}

vfs_context proc_get_file_context(proc_handle pid)
{
  return processes[pid].file_context;
}

static uint32_t syscall_exit(int exit_status)
{
  proc_kill(*proc_current_pid(), exit_status);
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
  verify_ptr(proc, filename);
  dbg_puts(proc, "spawning process with image '%s'", filename);
  // TODO: handle flags
  proc_handle pid = proc_create(PROC_USER_SPACE, "");
  elf_map_process(pid, filename);
  proc_enqueue(pid);
  return pid;
}

static int syscall_exec(const char *filename, const char *us_argv[])
{
  verify_ptr(proc, filename);   // TODO: verify whole filename
  verify_ptr(proc, us_argv);

  dbg_puts(proc, "execing process with image '%s'", filename);

  // User space will be unmapped further down, and to skip copying
  // arguments into the kernel we'll setup the new user stack
  // first. The drawback is that if anything fails after assigning the
  // stack, the process is broken.
  // TODO: make this safer in the case of errors

  const char *new_argv[32] = {};
  const char **in_arg = us_argv, **out_arg = new_argv;

  *out_arg++ = filename;

  while (*in_arg) {
    verify_ptr(proc, *in_arg);
    // TODO: verify string
    *out_arg++ = *in_arg++;
  }

  const int argc = out_arg - new_argv;

  // Overwrite user stack with program arguments. This is the "point of no return"!
  proc_assign_user_stack(*proc_current_pid(), new_argv, argc);

  // Copy filename into the kernel stack so we can swap out user space
  p2::string<128> image_path{filename};

  // Remove existing user space mappings so there won't be any collisions
  mem_unmap_not_matching(proc_get_space(*proc_current_pid()), MEM_AREA_RETAIN_EXEC);

  // Keep files like stdout and mmap'd elf files
  vfs_close_not_matching(proc_get_file_context(*proc_current_pid()), OPEN_RETAIN_EXEC);

  if (int result = elf_map_process(*proc_current_pid(), image_path.c_str()); result < 0) {
    return result;
  }

  return 0;
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

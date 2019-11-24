#include "process.h"
#include "protected_mode.h"
#include "screen.h"
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
#include "support/assert.h"

#include "process_private.h"

SYSCALL_DEF1(exit, SYSCALL_NUM_EXIT, uint32_t);

// Externals
extern "C" void isr_timer(isr_registers *);

// Statics
static uint32_t    syscall_yield();
static uint32_t    syscall_exit(int exit_code);
static uint32_t    syscall_kill(uint32_t pid);
static int         syscall_exec(const char *filename, const char **argv);
static int         syscall_fork(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, isr_registers *regs);
static int         syscall_shutdown();
static int         syscall_wait(int pid);
static int         syscall_set_timeout(int timeout);

static proc_handle decide_next_process();
static void        destroy_process(proc_handle pid);
static void        switch_process(proc_handle pid);

static void        enqueue_front(proc_handle pid, proc_handle *head);
static void        dequeue(proc_handle pid, proc_handle *head);

static void        idle_main();

// Global state
static p2::pool<process, 128, proc_handle> processes;

static proc_handle current_pid = processes.end();
static proc_handle running_head = processes.end(), suspended_head = processes.end();
static proc_handle idle_process = processes.end();

static uint64_t tick_count;

// Definitions
void proc_init()
{
  // Syscalls
  syscall_register(SYSCALL_NUM_YIELD,       (syscall_fun)syscall_yield);
  syscall_register(SYSCALL_NUM_EXIT,        (syscall_fun)syscall_exit);
  syscall_register(SYSCALL_NUM_KILL,        (syscall_fun)syscall_kill);
  syscall_register(SYSCALL_NUM_EXEC,        (syscall_fun)syscall_exec);
  syscall_register(SYSCALL_NUM_FORK,        (syscall_fun)syscall_fork);
  syscall_register(SYSCALL_NUM_SHUTDOWN,    (syscall_fun)syscall_shutdown);
  syscall_register(SYSCALL_NUM_WAIT,        (syscall_fun)syscall_wait);
  syscall_register(SYSCALL_NUM_SET_TIMEOUT, (syscall_fun)syscall_set_timeout);

  // Timer for preemptive task switching
  pit_set_phase(10);  // 9 is high freq, 10 is lower
  irq_enable(IRQ_SYSTEM_TIMER);
  int_register(IRQ_BASE_INTERRUPT + IRQ_SYSTEM_TIMER, isr_timer, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P);

  idle_process = proc_create(PROC_USER_SPACE|PROC_KERNEL_ACCESSIBLE);

  const char *args[] = {};
  proc_setup_user_stack(idle_process, 0, args);
  proc_set_syscall_ret(idle_process, (uintptr_t)idle_main);
}

proc_handle proc_create(uint32_t flags)
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
  processes[pid].setup_kernel_stack(nullptr);
  return pid;
}

void proc_setup_user_stack(proc_handle pid, int argc, const char *argv[])
{
  processes[pid].setup_user_stack(argc, argv);
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
  proc.destroy();

  // TODO: we might want to keep the PCB around for a while so we can
  // read the exit status, detect dangling references, etc...
  processes.erase(pid);
}


p2::opt<proc_handle> proc_current_pid()
{
  if (current_pid != processes.end())
    return current_pid;

  return {};
}

void proc_run()
{
  // Let the mayhem begin
  asm volatile("sti");
  proc_yield();

  // We'll never get here, but for good measure: a loop!
  // The real idle loop is in `idle_main`
  while (true) {}
}

extern "C" void int_timer(isr_registers *)
{
  proc_handle proc = suspended_head;
  irq_eoi(IRQ_SYSTEM_TIMER);

  while (proc != processes.end()) {
    process &process_ = processes[proc];

    if (process_.suspension_timeout > 0) {
      process_.suspension_timeout -= 1;  // TODO: correct tick

      if (process_.suspension_timeout <= 0) {
        // The timeout reached 0 so wake it up
        dbg_puts(proc, "unblocking %d due to timeout", proc);
        proc_unblock_and_switch(proc, ETIMEOUT);
      }
    }

    proc = process_.next_process;
  }

  proc_yield();
}

void proc_switch(proc_handle pid)
{
  if (proc_current_pid() && *proc_current_pid() == pid)
    return;

  process &proc = processes[pid];
  assert(!proc.suspended && "please resume the process before switching to it");

  if (proc.terminating) {
    dbg_puts(proc, "trying to switch to terminating process %d", pid);
    // The process has been terminated, so we cannot run it. This can
    // happen if the process was in a blocking syscall when someone
    // else asked to kill it.
    pid = decide_next_process();
  }

  switch_process(pid);
}

static void switch_process(proc_handle pid)
{
  dbg_puts(proc, "switching to pid %d", pid);

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

int proc_yield()
{
  // TODO: pull this tick count out into another kind of tick
  tick_count++;
  // Increase tick count so we get more fair sharing

  if (processes.valid(current_pid))
    processes[current_pid].last_tick = tick_count;

  proc_switch(decide_next_process());
  return processes[current_pid].unblock_status;
}

int proc_block(proc_handle pid)
{
  proc_suspend(pid);
  int ret = proc_yield();
  return ret;
}

void proc_unblock_and_switch(proc_handle pid, int status)
{
  if (processes.valid(pid)) {
    processes[pid].unblock_status = status;
    proc_resume(pid);
    proc_switch(pid);
  }
}

void proc_suspend(proc_handle pid)
{
  process &proc = processes[pid];
  if (proc.suspended) {
    return;
  }

  dbg_puts(proc, "suspending %d", pid);
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

  dbg_puts(proc, "resuming %d", pid);
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
  assert(pid != idle_process && "trying to kill the idle process");

  if (!processes.valid(pid)) {
    dbg_puts(proc, "pid %d is invalid", pid);
    return;
  }

  process &proc = processes[pid];

  // Remove the process so it won't get picked for execution
  dequeue(pid, proc.suspended ? &suspended_head : &running_head);

  // Mark the process as terminating, but don't clean it up. Some
  // other process should be waiting on this process, and it needs to
  // clean it up because you cannot destroy the currently active
  // address space
  proc.exit(exit_status);
  dbg_puts(proc, "pid %d exited with status %d", pid, exit_status);
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

template<int N>
static int copy_argvs(const char *argv[], p2::string<N> &arena, const char *pointers[], size_t ptrs_size)
{
  const char **argument = argv;
  size_t out_ptr_idx = 0;

  while (*argument && out_ptr_idx < ptrs_size) {
    verify_ptr(proc, *argument);
    pointers[out_ptr_idx++] = &arena[arena.size()];
    arena.append(*argument++);
    arena.append('\0');
  }

  return argument - argv;
}

static int syscall_exec(const char *filename, const char *argv[])
{
  verify_ptr(proc, filename);   // TODO: verify whole filename
  verify_ptr(proc, argv);

  dbg_puts(proc, "execing process with image '%s'", filename);

  vfs_context file_context = proc_get_file_context(*proc_current_pid());
  mem_space space = proc_get_space(*proc_current_pid());

  {
    // TODO: open the file but keep it open and send it to elf_map_process! Then remove RETAIN_EXEC
    p2::res<vfs_fd> result = vfs_open(file_context, filename, 0);
    if (!result)
      return result.error();
    else
      vfs_close(file_context, *result);
  }

  // Copy strings to the kernel stack so we can swap out user space
  p2::string<128> image_path{filename};
  p2::string<1024> arg_arena;

  const char *arg_ptrs[32];
  int argc = copy_argvs(argv, arg_arena, arg_ptrs, ARRAY_SIZE(arg_ptrs));
  arg_ptrs[argc + 1] = nullptr;

  // Remove existing user space mappings so there won't be any
  // collisions. This is where the old stack goes away.
  mem_unmap_not_matching(space, MEM_AREA_RETAIN_EXEC);

  // Keep files like stdout and mmap'd binaries
  vfs_close_not_matching(file_context, OPEN_RETAIN_EXEC);

  if (int result = elf_map_process(*proc_current_pid(), image_path.c_str()); result < 0) {
    return result;
  }

  // Overwrite user stack with program arguments
  proc_setup_user_stack(*proc_current_pid(), argc, arg_ptrs);

  mem_print_space(space);

  dbg_puts(proc, "exec successful");

  return 0;
}

static int syscall_fork(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, isr_registers *regs)
{
  proc_handle parent_pid = *proc_current_pid();
  dbg_puts(proc, "forking process %d", parent_pid);

  mem_space space_handle = *mem_fork_space(proc_get_space(parent_pid));
  vfs_context file_context = *vfs_fork_context(proc_get_file_context(parent_pid));

  proc_handle child_pid = processes.emplace_back(space_handle, file_context, 0);
  dbg_puts(proc, "... forked child pid: %d", child_pid);

  processes[child_pid].setup_kernel_stack(regs);
  proc_enqueue(child_pid);
  return child_pid;
}

static int syscall_shutdown()
{
  // TODO: destroy all processes
  // TODO: correct ACPI, this just works for emulators...
  dbg_puts(proc, "shutting down...");
  outw(0xB004, 0x2000); // Bochs and older versions of QEMU
  outw(0x604, 0x2000);  // Newer versions of QEMU
  outw(0x4004, 0x3400); // Virtualbox

  while (true) {}
}

static int syscall_wait(int pid)
{
  // TODO: only allow waiting on children
  if (!processes.valid(pid)) {
    dbg_puts(proc, "wait: %d not valid", pid);
    return 0;
  }

  if (auto parent_pid = proc_current_pid()) {
    if (!processes[pid].terminating) {
      proc_suspend(*parent_pid);
      processes[pid].waiting_process = *parent_pid;
      proc_yield();
      dbg_puts(proc, "came back after wait");
      assert(processes[pid].terminating && "woke up without pid terminating");
    }

    // TODO: save exit status
    destroy_process(pid);
  }

  return 1;
}

static int syscall_set_timeout(int timeout)
{
  if (auto pid = proc_current_pid()) {
    processes[*pid].suspension_timeout = timeout;
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

  // NB: this should *never* return -- the CPU will just continue and execute random stuff
}

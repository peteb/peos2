// -*- c++ -*-

#ifndef PEOS2_PROCESS_PRIVATE_H
#define PEOS2_PROCESS_PRIVATE_H

#include "memareas.h"
#include "process.h"
#include "stack_portal.h"

#include "support/utils.h"
#include "support/optional.h"

// Externs
extern "C" void switch_task_iret();
extern "C" void switch_task(uint32_t *old_esp, uint32_t new_esp, uintptr_t page_dir);
extern "C" void _user_proc_cleanup();

// Constants
static const uint16_t kernel_stack_flags = MEM_AREA_READWRITE|MEM_AREA_NO_FORK|MEM_AREA_RETAIN_EXEC;
static const uint16_t user_stack_flags = MEM_AREA_READWRITE|MEM_AREA_USER|MEM_AREA_SYSCALL;

static const size_t kernel_initial_stack_size = 0x1000 * 10;
static const size_t user_initial_stack_size = 0x1000;

//
// process - contains state and resources that belong to a process
//
class process : p2::non_copyable {
public:
  process(mem_space space_handle, vfs_context file_context, uint32_t flags)
    : space_handle(space_handle), file_context(file_context), _flags(flags) {}

  //
  // setup_stacks - allocates and initializes kernel stack
  //
  void setup_kernel_stack(isr_registers *regs)
  {
    const size_t stack_area_size = 0x1000 * 10;

    // TODO: kernel stacks cannot actually grow using page faults,
    // because the page fault will be pushed on the kernel stack,
    // causing another page fault, then the tripple fault is a fact
    mem_map_alloc(space_handle,
                  PROC_KERNEL_STACK_BASE - stack_area_size,
                  PROC_KERNEL_STACK_BASE,
                  kernel_stack_flags);

    _kernel_stack_sp = write_kernel_stack(regs);
  }

  //
  // setup_user_stack - initializes the user stack
  //
  void setup_user_stack(int argc, const char *argv[])
  {
    const size_t stack_area_size = 0x1000 * 1024;
    // The user stack can grow using page faults, so we don't need to
    // oversubscribe
    mem_map_alloc(space_handle,
                  USER_SPACE_STACK_BASE - stack_area_size,
                  USER_SPACE_STACK_BASE,
                  user_stack_flags);

    uintptr_t user_stack_ptr = write_user_stack(argc, argv);
    update_userspace_sp(user_stack_ptr);
  }

  //
  // The process has exited
  //
  void exit(int status)
  {
    terminating = true;
    exit_status = status;

    if (waiting_process) {
      proc_resume(*waiting_process);
    }
  }

  void destroy()
  {
    dbg_puts(proc, "destroying process");
    vfs_destroy_context(file_context);
    mem_destroy_space(space_handle);
    // TODO: invalidate handles
  }

  //
  // activate - executes a full context switch
  //
  void activate(process *previous_proc)
  {
    uint32_t *current_task_esp = 0;
    uint32_t **current_task_esp_ptr = &current_task_esp;

    // Save the old process' kernel stack pointer. But only if the old
    // process still exists.
    if (previous_proc) {
      current_task_esp_ptr = (uint32_t **)&previous_proc->_kernel_stack_sp;
    }

    tss_set_kernel_stack((uintptr_t)PROC_KERNEL_STACK_BASE);
    uintptr_t page_dir = mem_page_dir(space_handle);
    mem_set_current_space(space_handle);
    switch_task((uint32_t *)current_task_esp_ptr, _kernel_stack_sp, page_dir);
  }

  //
  // set_syscall_ret_ip - which address in user space the current syscall should return to
  //
  void set_syscall_ret_ip(uintptr_t ip)
  {
    stack_portal kernel_stack(space_handle, PROC_KERNEL_STACK_BASE, 0x1000);
    assert(kernel_stack[-1] == USER_DATA_SEL);
    assert(kernel_stack[-4] == USER_CODE_SEL);
    kernel_stack[-5] = ip;
  }

  mem_space   space_handle;
  vfs_context file_context;

  bool        suspended = false;
  bool        terminating = false;
  int         exit_status = 0, unblock_status = 0;

  proc_handle prev_process;
  proc_handle next_process;

  p2::opt<proc_handle> waiting_process;

  uint64_t    last_tick = 0;

private:
  uint32_t _flags;
  uintptr_t _kernel_stack_sp;

  //
  // write_kernel_stack - uses @regs if non-null. Returns the new
  // stack pointer in target address space
  //
  uintptr_t write_kernel_stack(isr_registers *regs)
  {
    stack_portal kernel_stack(space_handle, PROC_KERNEL_STACK_BASE, kernel_initial_stack_size);

    // We need a stack that can invoke iret as soon as possible, without
    // invoking any gcc function epilogues or prologues.  First, we need
    // a stack good for `switch_task`. As we set the return address to
    // be `switch_task_iret`, we also need to push values for IRET.
    kernel_stack.push(USER_DATA_SEL);                          // SS
    kernel_stack.push(regs ? regs->user_esp : 0);              // ESP
    kernel_stack.push(0x202);                                  // EFLAGS, IF
    kernel_stack.push(USER_CODE_SEL);                          // CS
    kernel_stack.push(regs ? regs->eip : 0);                   // Return EIP from switch_task_iret
    kernel_stack.push(USER_DATA_SEL);                          // DS, ES, FS, GS, SS
    kernel_stack.push((uint32_t)switch_task_iret);             // Return EIP from switch_task
    kernel_stack.push(0);                                      // EAX
    kernel_stack.push(regs ? regs->ecx : 0);                   // ECX
    kernel_stack.push(regs ? regs->edx : 0);                   // EDX
    kernel_stack.push(regs ? regs->ebx : 0);                   // EBX
    kernel_stack.push(0);                                      // ESP temp
    kernel_stack.push(regs ? regs->ebp : 0);                   // EBP
    kernel_stack.push(regs ? regs->esi : 0);                   // ESI
    kernel_stack.push(regs ? regs->edi : 0);                   // EDI

    return kernel_stack.target_address(kernel_stack.current_pointer());
  }

  //
  // write_user_stack - writes the stack and returns the new stack
  // pointer in target address space
  //
  uintptr_t write_user_stack(int argc, const char *argv[])
  {
    stack_portal user_stack(space_handle, USER_SPACE_STACK_BASE, user_initial_stack_size);

    // Copy arguments to the stack and rebase addresses
    char **arg_ptrs = (char **)user_stack.push((const char *)argv, sizeof(argv[0]) * argc);

    for (int i = 0; i < argc; ++i) {
      char *entry_pos = user_stack.push(argv[i], strlen(argv[i]) + 1);
      arg_ptrs[i] = (char *)user_stack.target_address((uintptr_t)entry_pos);
    }

    user_stack.push(user_stack.target_address((uintptr_t)arg_ptrs));
    user_stack.push(argc);

    // If the kernel is accessible, we'll add a cleanup function to
    // make sure the process is killed when its main function
    // returns. User space processes are on their own; they cannot
    // access this function anyway.
    if (_flags & PROC_KERNEL_ACCESSIBLE)
      user_stack.push((uintptr_t)_user_proc_cleanup);
    else
      user_stack.push(0);

    return user_stack.target_address(user_stack.current_pointer());
  }

  void update_userspace_sp(uintptr_t user_stack_ptr)
  {
    stack_portal kernel_stack(space_handle, PROC_KERNEL_STACK_BASE, 0x1000);
    assert(kernel_stack[-1] == USER_DATA_SEL);
    kernel_stack[-2] = user_stack_ptr;
  }
};


//
// Called in processes that have the kernel accessible.
//
extern "C" void _user_proc_cleanup()
{
  uint32_t retval;
  asm volatile("" : "=a"(retval));
  syscall1(exit, retval);
}


#endif // !PEOS2_PROCESS_PRIVATE_H

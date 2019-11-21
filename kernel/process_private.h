// -*- c++ -*-

#ifndef PEOS2_PROCESS_PRIVATE_H
#define PEOS2_PROCESS_PRIVATE_H

#include "memareas.h"
#include "process.h"
#include "support/utils.h"
#include "support/optional.h"

extern "C" void switch_task_iret();
extern "C" void switch_task(uint32_t *old_esp, uint32_t new_esp, uintptr_t page_dir);
extern "C" void _user_proc_cleanup();

class stack {
public:
  stack() : sp(nullptr), base(nullptr), end(nullptr) {}

  stack(uint32_t *start, uint32_t *end)
    : sp(end), base(end), end(start)
  {
    // Sorry for the confusing name "end" ...
    assert(start < end);
  }

  void push(uint32_t value)
  {
    assert(sp <= base);
    assert(sp > end);
    *--sp = value;
  }

  char *push(const char *data, size_t count)
  {
    assert(sp <= base);
    assert((uintptr_t)sp - (uintptr_t)count > (uintptr_t)end);

    char *dest = (char *)sp - count;
    memcpy(dest, data, count);
    sp = (uint32_t *)ALIGN_DOWN((uintptr_t)dest, sizeof(*sp));
    return dest;
  }

  size_t bytes_used() const
  {
    return (base - sp) * sizeof(*sp);
  }

  uint32_t *sp, *base, *end;
};

static const uint16_t kernel_stack_flags = MEM_AREA_READWRITE|MEM_AREA_NO_FORK|MEM_AREA_RETAIN_EXEC;
static const uint16_t user_stack_flags = MEM_AREA_READWRITE|MEM_AREA_USER|MEM_AREA_SYSCALL;

//
// process - contains state and resources that belongs to a process.
//
class process : p2::non_copyable {
public:
  process(mem_space space_handle, vfs_context file_context, uint32_t flags)
    : space_handle(space_handle), file_context(file_context), flags(flags) {}

  //
  // setup_stacks - allocates and initializes kernel stack
  //
  void setup_stack()
  {
    map_kernel_stack();

    // Map the first page of the user stack -- we shouldn't need much
    // more for initialization unless there's a ton of arguments
    const size_t initial_stack_size = 0x1000;

    int retval = mem_map_portal(KERNEL_SCRATCH_BASE,
                                initial_stack_size,
                                space_handle,
                                PROC_KERNEL_STACK_BASE - initial_stack_size,
                                kernel_stack_flags);
    assert(retval >= 0 && "failed to map portal");

    // KERNEL_SCRATCH_BASE + 0x1000 is where the stack starts at and grows down
    stack kernel_stack((uint32_t *)KERNEL_SCRATCH_BASE, (uint32_t *)(KERNEL_SCRATCH_BASE + initial_stack_size));

    // We need a stack that can invoke iret as soon as possible, without
    // invoking any gcc function epilogues or prologues.  First, we need
    // a stack good for `switch_task`. As we set the return address to
    // be `switch_task_iret`, we also need to push values for IRET.
    kernel_stack.push(USER_DATA_SEL);                                // SS
    kernel_stack.push(0);                                            // ESP, should be filled in later
    kernel_stack.push(0x202);                                        // EFLAGS, IF
    kernel_stack.push(USER_CODE_SEL);                                // CS
    kernel_stack.push(0);                                            // Return EIP from switch_task_iret, should be filled in later
    kernel_stack.push(USER_DATA_SEL);                                // DS, ES, FS, GS, SS
    kernel_stack.push((uint32_t)switch_task_iret);                   // Return EIP from switch_task
    kernel_stack.push(0);                                            // EAX
    kernel_stack.push(0);                                            // ECX
    kernel_stack.push(0);                                            // EDX
    kernel_stack.push(0);                                            // EBX
    kernel_stack.push(0);                                            // ESP temp
    kernel_stack.push(0);                                            // EBP
    kernel_stack.push(0);                                            // ESI
    kernel_stack.push(0);                                            // EDI

    kernel_stack_sp = PROC_KERNEL_STACK_BASE - kernel_stack.bytes_used();

    mem_unmap_portal(KERNEL_SCRATCH_BASE, initial_stack_size);
  }

  void setup_fork_stack(isr_registers *regs)
  {
    map_kernel_stack();

    // Map the first page of the user stack -- we shouldn't need much
    // more for initialization unless there's a ton of arguments, but
    // we need to reserve space due to not being able to grow using
    // page faults
    const size_t initial_stack_size = 0x1000 * 10;

    int retval = mem_map_portal(KERNEL_SCRATCH_BASE,
                                initial_stack_size,
                                space_handle,
                                PROC_KERNEL_STACK_BASE - initial_stack_size,
                                kernel_stack_flags);
    assert(retval >= 0 && "failed to map portal");

    // KERNEL_SCRATCH_BASE + 0x1000 is where the stack starts at and grows down
    stack kernel_stack((uint32_t *)KERNEL_SCRATCH_BASE, (uint32_t *)(KERNEL_SCRATCH_BASE + initial_stack_size));
    // We need a stack that can invoke iret as soon as possible, without
    // invoking any gcc function epilogues or prologues.  First, we need
    // a stack good for `switch_task`. As we set the return address to
    // be `switch_task_iret`, we also need to push values for IRET.
    kernel_stack.push(USER_DATA_SEL);                                // SS
    kernel_stack.push(regs->user_esp);                               // ESP
    kernel_stack.push(0x202);                                        // EFLAGS, IF
    kernel_stack.push(USER_CODE_SEL);                                // CS
    kernel_stack.push(regs->eip);                                    // Return EIP
    kernel_stack.push(USER_DATA_SEL);                                // DS, ES, FS, GS, SS
    kernel_stack.push((uint32_t)switch_task_iret);                   // Return EIP from switch_task
    kernel_stack.push(0);                                            // EAX
    kernel_stack.push(regs->ecx);                                    // ECX
    kernel_stack.push(regs->edx);                                    // EDX
    kernel_stack.push(regs->ebx);                                    // EBX
    kernel_stack.push(0);                                            // ESP temp
    kernel_stack.push(regs->ebp);                                    // EBP
    kernel_stack.push(regs->esi);                                    // ESI
    kernel_stack.push(regs->edi);                                    // EDI

    kernel_stack_sp = PROC_KERNEL_STACK_BASE - kernel_stack.bytes_used();

    mem_unmap_portal(KERNEL_SCRATCH_BASE, initial_stack_size);
    // TODO: support for setting eax value?
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
  }

  //
  // setup_user_stack - overwrites the user stack
  //
  void setup_user_stack(int argc, const char *argv[])
  {
    map_user_stack();  // This can fail with a panic if the stack is mapped already

    // Map the first page of the user stack -- we shouldn't need much
    // more for initialization unless there's a ton of arguments
    const size_t initial_stack_size = 0x1000;

    int retval = mem_map_portal(KERNEL_SCRATCH_BASE,
                                initial_stack_size,
                                space_handle,
                                USER_SPACE_STACK_BASE - initial_stack_size,
                                user_stack_flags);
    assert(retval >= 0 && "failed to map portal");

    // KERNEL_SCRATCH_BASE + 0x1000 is where the stack starts at and grows down
    stack user_stack((uint32_t *)KERNEL_SCRATCH_BASE, (uint32_t *)(KERNEL_SCRATCH_BASE + initial_stack_size));

    char **arg_ptrs = (char **)user_stack.push((const char *)argv, sizeof(argv[0]) * argc);

    for (int i = 0; i < argc; ++i) {
      arg_ptrs[i] = (char *)virt_user_sp(user_stack, user_stack.push(argv[i], strlen(argv[i]) + 1));
    }

    user_stack.push(virt_user_sp(user_stack, (char *)arg_ptrs));
    user_stack.push(argc);

    // If the kernel is accessible, we'll add a cleanup function to make
    // sure the process is killed when its main function returns. User
    // space processes are on their own.
    if (flags & PROC_KERNEL_ACCESSIBLE)
      user_stack.push((uintptr_t)_user_proc_cleanup);
    else
      user_stack.push(0);

    update_userspace_sp(user_stack.bytes_used());
    mem_unmap_portal(KERNEL_SCRATCH_BASE, initial_stack_size);
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
      current_task_esp_ptr = (uint32_t **)&previous_proc->kernel_stack_sp;
    }

    tss_set_kernel_stack((uintptr_t)PROC_KERNEL_STACK_BASE);
    uintptr_t page_dir = mem_page_dir(space_handle);
    mem_set_current_space(space_handle);
    switch_task((uint32_t *)current_task_esp_ptr, kernel_stack_sp, page_dir);
  }

  //
  // set_syscall_ret_ip - which address in user space the current syscall should return to
  //
  void set_syscall_ret_ip(uintptr_t ip)
  {
    int ret = mem_map_portal(KERNEL_SCRATCH_BASE,
                             0x1000,
                             space_handle,
                             PROC_KERNEL_STACK_BASE - 0x1000,
                             kernel_stack_flags);
    assert(ret >= 0);

    uint32_t *stack_base = (uint32_t *)(KERNEL_SCRATCH_BASE + 0x1000);
    assert((uint32_t *)kernel_stack_sp <= &stack_base[-5]);
    assert(stack_base[-1] == USER_DATA_SEL);
    assert(stack_base[-4] == USER_CODE_SEL);
    stack_base[-5] = ip;

    mem_unmap_portal(KERNEL_SCRATCH_BASE, 0x1000);
  }

  mem_space space_handle;
  vfs_context file_context;

  bool        suspended = false;
  bool        terminating = false;
  int         exit_status;

  proc_handle prev_process;
  proc_handle next_process;

  p2::opt<proc_handle> waiting_process;

  uint64_t    last_tick = 0;

private:
  uint32_t flags;

  uintptr_t kernel_stack_sp;

  void update_userspace_sp(size_t user_stack_size)
  {
    int ret = mem_map_portal(KERNEL_SCRATCH_BASE,
                             0x1000,
                             space_handle,
                             PROC_KERNEL_STACK_BASE - 0x1000,
                             kernel_stack_flags);
    assert(ret >= 0);

    uint32_t *stack_base = (uint32_t *)(KERNEL_SCRATCH_BASE + 0x1000);
    assert(stack_base[-1] == USER_DATA_SEL);
    stack_base[-2] = USER_SPACE_STACK_BASE - user_stack_size;

    mem_unmap_portal(KERNEL_SCRATCH_BASE, 0x1000);
  }

  uintptr_t virt_user_sp(stack &user_stack, const char *sp)
  {
    return USER_SPACE_STACK_BASE - ((uintptr_t)user_stack.base - (uintptr_t)sp);
  }

  void map_user_stack()
  {
    const size_t stack_area_size = 0x1000 * 1024;
    mem_map_alloc(space_handle,
                  USER_SPACE_STACK_BASE - stack_area_size,
                  USER_SPACE_STACK_BASE,
                  user_stack_flags);
  }

  void map_kernel_stack()
  {
    const size_t stack_area_size = 0x1000 * 10;
    // TODO: kernel stack cannot actually grow using page faults, because the page fault will be
    // pushed on the kernel stack, causing another page fault, then the tripple fault is a fact
    mem_map_alloc(space_handle,
                  PROC_KERNEL_STACK_BASE - stack_area_size,
                  PROC_KERNEL_STACK_BASE,
                  kernel_stack_flags);
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

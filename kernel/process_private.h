// -*- c++ -*-

#ifndef PEOS2_PROCESS_PRIVATE_H
#define PEOS2_PROCESS_PRIVATE_H

#include "memareas.h"
#include "process.h"
#include "support/utils.h"
#include "support/optional.h"

extern "C" void switch_task_iret();
extern "C" void switch_task(uint32_t *old_esp, uint32_t new_esp);
extern "C" void _user_proc_cleanup();


//
// Storage for kernel and user stacks.
//
template<size_t N>
class stack_storage {
public:
  uint32_t *base()
  {
    size_t length = ARRAY_SIZE(_data);
    assert(length > 0);
    return &_data[length];
    // &_data[length] should be correct, ie, one above the end of the
    // array: it's explicitly a legal pointer in C/C++, and x86 PUSH
    // first decrements ESP before writing to [ESP].
  }

  uint32_t *start()
  {
    return &_data[0];
  }

  // User stacks need to be aligned on 4096 byte boundaries as we map
  // them into the process. They also need to be aligned on 16 bytes.
  uint32_t _data[N] alignas(0x1000);
};

class stack {
public:
  stack() : sp(nullptr), base(nullptr), end(nullptr) {}

  template<size_t N>
  stack(stack_storage<N> &storage)
    : sp(storage.base()), base(storage.base()), end(storage.base() - N)
  {}

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

  void rewind()
  {
    sp = base;
  }

  size_t bytes_used() const
  {
    return (base - sp) * sizeof(*sp);
  }

  uint32_t *sp, *base, *end;
};

static p2::pool<stack_storage<512>, 128> kernel_stacks;

//
// process - contains state and resources that belongs to a process.
//
class process : p2::non_copyable {
public:
  process(mem_space space_handle, vfs_context file_context, uint32_t flags)
    : space_handle(space_handle), file_context(file_context), flags(flags) {}

  const uintptr_t USER_SPACE_STACK_BASE = 0xB0000000;

  //
  // setup_stacks - allocates and initializes user and kernel stacks
  //
  void setup_stack()
  {
    assert(!kernel_stack.base);

    kernel_stack_handle = kernel_stacks.emplace_back();
    kernel_stack = {kernel_stacks[kernel_stack_handle]};

    // We need a stack that can invoke iret as soon as possible, without
    // invoking any gcc function epilogues or prologues.  First, we need
    // a stack good for `switch_task`. As we set the return address to
    // be `switch_task_iret`, we also need to push values for IRET.
    ks_push(USER_DATA_SEL);                                // SS
    ks_push(0);                                            // ESP, should be filled in later
    ks_push(0x202);                                        // EFLAGS, IF
    ks_push(USER_CODE_SEL);                                // CS
    ks_push(0);                                            // Return EIP from switch_task_iret, should be filled in later
    ks_push(USER_DATA_SEL);                                // DS, ES, FS, GS, SS
    ks_push((uint32_t)switch_task_iret);                   // Return EIP from switch_task
    ks_push(0);                                            // EAX
    ks_push(0);                                            // ECX
    ks_push(0);                                            // EDX
    ks_push(0);                                            // EBX
    ks_push(0);                                            // ESP temp
    ks_push(0);                                            // EBP
    ks_push(0);                                            // ESI
    ks_push(0);                                            // EDI
  }

  void setup_fork_stack(isr_registers *regs)
  {
    assert(!kernel_stack.base);

    kernel_stack_handle = kernel_stacks.emplace_back();
    kernel_stack = {kernel_stacks[kernel_stack_handle]};

    // We need a stack that can invoke iret as soon as possible, without
    // invoking any gcc function epilogues or prologues.  First, we need
    // a stack good for `switch_task`. As we set the return address to
    // be `switch_task_iret`, we also need to push values for IRET.
    ks_push(USER_DATA_SEL);                                // SS
    ks_push(regs->user_esp);                               // ESP
    ks_push(0x202);                                        // EFLAGS, IF
    ks_push(USER_CODE_SEL);                                // CS
    ks_push(regs->eip);                                    // Return EIP
    ks_push(USER_DATA_SEL);                                // DS, ES, FS, GS, SS
    ks_push((uint32_t)switch_task_iret);                   // Return EIP from switch_task
    ks_push(0);                                            // EAX
    ks_push(regs->ecx);                                    // ECX
    ks_push(regs->edx);                                    // EDX
    ks_push(regs->ebx);                                    // EBX
    ks_push(0);                                            // ESP temp
    ks_push(regs->ebp);                                    // EBP
    ks_push(regs->esi);                                    // ESI
    ks_push(regs->edi);                                    // EDI

    // TODO: support for setting eax value?
  }

  //
  // free_stack - hands back storage for the kernel stack
  //
  void free_stack()
  {
    // Free up memory used by the process
    kernel_stacks.erase(kernel_stack_handle);
    kernel_stack_handle = kernel_stacks.end();
    kernel_stack = {};
  }

  void destroy()
  {
    vfs_destroy_context(file_context);
    mem_destroy_space(space_handle);
    free_stack();

    if (waiting_process) {
      proc_resume(*waiting_process);
    }
  }

  //
  // setup_user_stack - overwrites the user stack
  //
  void setup_user_stack(int argc, const char *argv[])
  {
    map_user_stack();  // This can fail with a panic if the stack is mapped already

    stack_storage<0x1000 / sizeof(uint32_t)> user_stack_storage; // TODO: shouldn't have to divide here
    stack user_stack(user_stack_storage);

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

    mem_write_page(space_handle,
                   USER_SPACE_STACK_BASE - 0x1000,
                   user_stack_storage.start(),
                   0x1000);

    update_ks_ret_sp(user_stack.bytes_used());
  }

  //
  // set_syscall_ret_ip - which address in user space the current syscall should return to
  //
  void set_syscall_ret_ip(uintptr_t ip)
  {
    assert(kernel_stack.sp <= kernel_stack.base - 5);
    assert(kernel_stack.base[-1] == USER_DATA_SEL);
    assert(kernel_stack.base[-4] == USER_CODE_SEL);
    kernel_stack.base[-5] = ip;
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
      current_task_esp_ptr = &previous_proc->kernel_stack.sp;
    }

    tss_set_kernel_stack((uintptr_t)kernel_stack.base);
    mem_activate_space(space_handle);
    switch_task((uint32_t *)current_task_esp_ptr, (uintptr_t)kernel_stack.sp);
  }

  mem_space space_handle;
  vfs_context file_context;

  bool        suspended = false;
  bool        terminating = false;

  proc_handle prev_process;
  proc_handle next_process;

  p2::opt<proc_handle> waiting_process;

  uint64_t    last_tick = 0;

private:
  uint32_t flags;

  stack    kernel_stack;
  uint16_t kernel_stack_handle;

  void ks_push(uint32_t value)
  {
    kernel_stack.push(value);
  }

  void update_ks_ret_sp(size_t user_stack_size)
  {
    assert(kernel_stack.sp <= kernel_stack.base - 1);
    assert(kernel_stack.base[-1] == USER_DATA_SEL);
    kernel_stack.base[-2] = USER_SPACE_STACK_BASE - user_stack_size;
  }

  uintptr_t virt_user_sp(stack &user_stack, const char *sp)
  {
    return USER_SPACE_STACK_BASE - ((uintptr_t)user_stack.base - (uintptr_t)sp);
  }

  void map_user_stack()
  {
    const size_t stack_area_size = 4096 * 1024;
    mem_map_alloc(space_handle,
                  USER_SPACE_STACK_BASE - stack_area_size,
                  USER_SPACE_STACK_BASE,
                  MEM_AREA_READWRITE|MEM_AREA_USER|MEM_AREA_SYSCALL);
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

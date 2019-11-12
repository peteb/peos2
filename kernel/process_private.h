// -*- c++ -*-

#ifndef PEOS2_PROCESS_PRIVATE_H
#define PEOS2_PROCESS_PRIVATE_H

#include "memareas.h"
#include "support/utils.h"

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

static p2::pool<stack_storage<0x1000>, 16> user_stacks;
static p2::pool<stack_storage<256>, 16> kernel_stacks;


//
// process - contains state and resources that belongs to a process.
//
class process : p2::non_copyable {
public:
  process(mem_space space_handle, uint32_t flags)
    : space_handle(space_handle), flags(flags) {}

  const uintptr_t USER_SPACE_STACK_BASE = 0xB0000000;

  //
  // setup_stacks - allocates and initializes user and kernel stacks
  //
  void setup_stacks()
  {
    assert(!user_stack.base);
    assert(!kernel_stack.base);

    kernel_stack_handle = kernel_stacks.emplace_back();
    kernel_stack = {kernel_stacks[kernel_stack_handle]};
    user_stack_handle = user_stacks.emplace_back();
    user_stack = {user_stacks[user_stack_handle]};

    // We need a stack that can invoke iret as soon as possible, without
    // invoking any gcc function epilogues or prologues.  First, we need
    // a stack good for `switch_task`. As we set the return address to
    // be `switch_task_iret`, we also need to push values for IRET.
    ks_push(USER_DATA_SEL);                                // SS
    ks_push(0);                                            // ESP, should be filled in later
    ks_push(0x202);                                        // EFLAGS, IF
    ks_push(USER_CODE_SEL);                                // CS
    ks_push(0);                                            // Return EIP from switch_task_iret, should be filled in later
    ks_push((uint32_t)switch_task_iret);                   // Return EIP from switch_task
    ks_push(0);                                            // EBX
    ks_push(0);                                            // ESI
    ks_push(0);                                            // EDI
    ks_push(0);                                            // EBP

    assign_user_stack(nullptr, 0);

    const size_t stack_area_size = 4096 * 1024;

    // The first page of the stack is in a kernel pool, this is an easy
    // way for the kernel to be able to populate the stack, albeit
    // wasteful. The rest of the stack pages will be allocated on the
    // fly.
    mem_map_linear(space_handle,
                   USER_SPACE_STACK_BASE - 0x1000,
                   USER_SPACE_STACK_BASE,
                   KERNVIRT2PHYS((uintptr_t)user_stack.base) - 0x1000,
                   MEM_AREA_READWRITE|MEM_AREA_USER|MEM_AREA_SYSCALL);

    mem_map_alloc(space_handle,
                  USER_SPACE_STACK_BASE - stack_area_size,
                  USER_SPACE_STACK_BASE - 0x1000,
                  MEM_AREA_READWRITE|MEM_AREA_USER|MEM_AREA_SYSCALL);
  }

  //
  // free_stacks - hands back storage for user and kernel stacks
  //
  void free_stacks()
  {
    // Free up memory used by the process
    user_stacks.erase(user_stack_handle);
    kernel_stacks.erase(kernel_stack_handle);
    user_stack_handle = user_stacks.end();
    kernel_stack_handle = kernel_stacks.end();
    user_stack = {};
    kernel_stack = {};
  }

  //
  // assign_user_stack - overwrites the user stack
  //
  void assign_user_stack(const char *argv[], int argc)
  {
    us_rewind();

    char **arg_ptrs = (char **)user_stack.push((const char *)argv, sizeof(argv[0]) * argc);

    for (int i = 0; i < argc; ++i) {
      arg_ptrs[i] = (char *)virt_user_sp(user_stack.push(argv[i], strlen(argv[i]) + 1));
    }

    us_push(virt_user_sp((char *)arg_ptrs));
    us_push(argc); // argc

    // If the kernel is accessible, we'll add a cleanup function to make
    // sure the process is killed when its main function returns. User
    // space processes are on their own.
    if (flags & PROC_KERNEL_ACCESSIBLE)
      us_push((uintptr_t)_user_proc_cleanup);
    else
      us_push(0);
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

    tss_set_kernel_stack((uint32_t)kernel_stack.base);
    mem_activate_space(space_handle);
    switch_task((uint32_t *)current_task_esp_ptr, (uint32_t)kernel_stack.sp);
  }

  mem_space space_handle;

  p2::pool<proc_fd, 32> file_descriptors;

  bool        suspended = false;
  bool        terminating = false;

  proc_handle prev_process;
  proc_handle next_process;

  uint64_t    last_tick = 0;

private:
  uint32_t flags;

  stack    kernel_stack;
  stack    user_stack;
  uint16_t kernel_stack_handle;
  uint16_t user_stack_handle;

  void ks_push(uint32_t value)
  {
    kernel_stack.push(value);
  }

  void us_push(uint32_t value)
  {
    user_stack.push(value);
    update_ks_ret_sp();
  }

  void us_rewind()
  {
    user_stack.rewind();
    update_ks_ret_sp();
  }

  void update_ks_ret_sp()
  {
    assert(kernel_stack.sp <= kernel_stack.base - 1);
    assert(kernel_stack.base[-1] == USER_DATA_SEL);
    kernel_stack.base[-2] = USER_SPACE_STACK_BASE - user_stack.bytes_used();
  }

  uintptr_t virt_user_sp(const char *sp)
  {
    return USER_SPACE_STACK_BASE - ((uintptr_t)user_stack.base - (uintptr_t)sp);
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

// -*- c++ -*-

#ifndef PEOS2_PROCESS_PRIVATE_H
#define PEOS2_PROCESS_PRIVATE_H

//
// Wrapper for a kernel or user stack.
//
template<size_t N>
struct stack {
  uint32_t *base() {
    size_t length = ARRAY_SIZE(data);
    assert(length > 0);
    return &data[length];
    // &data[length] should be correct, ie, one above the end of the
    // array: it's explicitly a legal pointer in C/C++, and x86 PUSH
    // first decrements ESP before writing to [ESP].
  }

  // User stacks need to be aligned on 4096 byte boundaries as we map
  // them into the process. They also need to be aligned on 16 bytes.
  uint32_t data[N] alignas(0x1000);
};

// TODO: this file needs to be cleaned up
class process_control_block {
public:
  process_control_block(mem_space space_handle)
    : space_handle(space_handle),
      suspended(false),  // TODO: move this into a status field
      terminating(false)
  {
    argv[0] = argument.c_str();
  }

  // TODO: extract into non-copyable and non-movable
  process_control_block(const process_control_block &other) = delete;
  process_control_block &operator =(const process_control_block &other) = delete;

  // TODO: right now the stack allocation logic is a bit split with
  // this file and process.c, make it nicer.

  template<size_t N>
  void set_kernel_stack(stack<N> &kernel_stack, uint16_t stack_handle)
  {
    kernel_esp = kernel_stack.base();
    kernel_stack_base = kernel_stack.base();
    kernel_stack_handle = stack_handle;
  }

  template<size_t N>
  void set_user_stack(stack<N> &user_stack, uint16_t stack_handle)
  {
    user_esp = user_stack.base();
    user_stack_handle = stack_handle;
  }

  void kpush(uint32_t value) {
    *--kernel_esp = value;
  }

  void upush(uint32_t value) {
    *--user_esp = value;
  }

  void activate() {
    tss_set_kernel_stack((uint32_t)kernel_stack_base);
    mem_activate_space(space_handle);
  }

  uint32_t *kernel_esp;
  uint32_t *kernel_stack_base;
  uint32_t *user_esp;

  const char *argv[1];
  p2::string<64> argument;

  mem_space space_handle;

  p2::pool<proc_fd, 32> file_descriptors;

  bool suspended, terminating;
  proc_handle prev_process, next_process;
  uint64_t last_tick = 0;
  uint16_t kernel_stack_handle, user_stack_handle;
};


#endif // !PEOS2_PROCESS_PRIVATE_H

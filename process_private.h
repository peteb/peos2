// -*- c++ -*-

#ifndef PEOS2_PROCESS_PRIVATE_H
#define PEOS2_PROCESS_PRIVATE_H

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

  p2::pool<proc_fd, 32> file_descriptors;

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
    return &data[length];
    // You can argue that &data[length] would be correct, ie, one
    // above the end of the array: it's explicitly a legal pointer in
    // C/C++, and x86 PUSH first decrements ESP before writing to
    // [ESP].
  }

  // Stacks need to be aligned on 4096 byte boundaries as we map them
  // into the process
  uint32_t data[N] alignas(0x1000);
};


#endif // !PEOS2_PROCESS_PRIVATE_H

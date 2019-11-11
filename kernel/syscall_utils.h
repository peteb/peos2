// -*- c++ -*-
#ifndef PEOS2_SYSCALL_UTILS_H
#define PEOS2_SYSCALL_UTILS_H

#include "process.h"
#include "memory.h"

static void kill_caller()
{
  proc_kill(*proc_current_pid(), 15);
  proc_yield();
}

static inline bool valid_pointer(const void *address)
{
  auto area_flags = mem_area_flags(proc_get_space(*proc_current_pid()), address);
  return area_flags && (*area_flags & MEM_AREA_SYSCALL);
}

#define verify_ptr(module, address)                                                           \
  if (!valid_pointer((address))) {                                                            \
    dbg_puts(module, "parameter '" TOSTRING(address) "' (%x) invalid", (uintptr_t)(address)); \
    kill_caller();                                                                            \
  }

#endif // !PEOS2_SYSCALL_UTILS_H

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

static inline bool valid_buffer(const void *address, size_t length)
{
  if (!valid_pointer(address))
    return false;

  mem_space current_space = proc_get_space(*proc_current_pid());
  auto start_area = mem_find_area(current_space, (uintptr_t)address);
  if (!start_area)
    return false;

  auto end_area = mem_find_area(current_space, (uintptr_t)address + length);
  if (!end_area)
    return false;

  return *start_area == *end_area;
}

#define verify_ptr(module, address)                                                           \
  if (!valid_pointer((address))) {                                                            \
    dbg_puts(module, "parameter '" TOSTRING(address) "' (%p) invalid", (uintptr_t)(address)); \
    kill_caller();                                                                            \
  }


#define verify_buf(module, address, sz)                                                                  \
  if (!valid_buffer((address), (sz))) {                                                                  \
    dbg_puts(module, "parameter '" TOSTRING(address) "' (%p:%08d) invalid", (uintptr_t)(address), (sz)); \
    kill_caller();                                                                                       \
  }


#endif // !PEOS2_SYSCALL_UTILS_H

// -*- c++ -*-

#ifndef PEOS2_STACK_PORTAL_H
#define PEOS2_STACK_PORTAL_H

#include <stddef.h>
#include <stdint.h>
#include "memory.h"

class stack_portal {
public:
  stack_portal(mem_space target_space, uintptr_t target_stack_base, size_t size_bytes)
    : _entry_mem_start(KERNEL_SCRATCH_BASE),
      _entry_mem_end(KERNEL_SCRATCH_BASE + ALIGN_UP(size_bytes, 0x1000)),
      _target_mem_start(target_stack_base - ALIGN_UP(size_bytes, 0x1000)),
      _target_mem_end(target_stack_base)
  {
    int retval = mem_map_portal(_entry_mem_start,
                                _entry_mem_end - _entry_mem_start,
                                target_space,
                                _target_mem_start,
                                MEM_AREA_READWRITE);
    assert(retval >= 0 && "failed to map portal");
    _stack_ptr = (uint32_t *)_entry_mem_end;
  }

  ~stack_portal()
  {
    mem_unmap_portal(_entry_mem_start, _entry_mem_end - _entry_mem_start);
  }

  uintptr_t target_address(uintptr_t entry_address)
  {
    assert(entry_address >= (uintptr_t)_entry_mem_start);
    assert(entry_address <= (uintptr_t)_entry_mem_end);
    return _target_mem_end - (_entry_mem_end - entry_address);
  }

  uintptr_t current_pointer()
  {
    return (uintptr_t)_stack_ptr;
  }

  void push(uint32_t value)
  {
    assert((uintptr_t)_stack_ptr <= _entry_mem_end);
    assert((uintptr_t)_stack_ptr > _entry_mem_start);
    *--_stack_ptr = value;
  }

  char *push(const char *data, size_t count)
  {
    assert((uintptr_t)_stack_ptr <= _entry_mem_end);
    assert((uintptr_t)_stack_ptr - (uintptr_t)count > _entry_mem_start);

    char *dest = (char *)_stack_ptr - count;
    memcpy(dest, data, count);
    _stack_ptr = (uint32_t *)ALIGN_DOWN((uintptr_t)dest, sizeof(*_stack_ptr));
    return dest;
  }

  size_t bytes_used() const
  {
    return _entry_mem_end - (uintptr_t)_stack_ptr;
  }

  uint32_t &operator [](int offset)
  {
    return _stack_ptr[offset];
  }

  uintptr_t _entry_mem_start, _entry_mem_end;
  uintptr_t _target_mem_start, _target_mem_end;
  uint32_t *_stack_ptr;
};

#endif // !PEOS2_STACK_PORTAL_H

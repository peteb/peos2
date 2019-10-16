// -*- c++ -*-

#ifndef PEOS2_MEM_RANGE_H
#define PEOS2_MEM_RANGE_H

#include <stdint.h>
#include <stddef.h>

#include "assert.h"

namespace p2 {
  template<typename T>
  class mem_range {
  public:
    mem_range(uintptr_t start, uintptr_t end)
      : start(reinterpret_cast<T *>(start)), end(reinterpret_cast<T *>(end)) {}
    mem_range(T *start, T *end)
      : start(start), end(end) {}

    void fill(T value) {
      // TODO: optimize using block operations
      T *pos = start;

      while (pos < end) {
        *pos++ = value;
      }
    }

    size_t size() const {
      return end - start;
    }

    T &operator [](int idx) {
      ASSERT(start + idx >= start && start + idx < end && "idx is within bounds");
      return start[idx];
    }

    const T &operator [](int idx) const {
      ASSERT(start + idx >= start && start + idx < end && "idx is within bounds");
      return start[idx];
    }

    mem_range subrange(int substart, int length = -1) {
      if (length == -1) {
        length = end - start;
      }

      return mem_range<T>(start + substart, start + substart + length);
    }

    void assign_overlap(const mem_range<T> &rhs) {
      size_t min_length = size();
      if (rhs.size() < min_length) {
        min_length = rhs.size();
      }

      // TODO: make this work when rhs is before this
      for (size_t i = 0; i < min_length; ++i) {
        (*this)[i] = rhs[i];
      }
    }

  private:
    T *start, *end;
  };
}

#endif // !PEOS2_MEM_RANGE_H

// -*- c++ -*-

#ifndef PEOS2_UTILS_H
#define PEOS2_UTILS_H

#include <stdint.h>
#include <stddef.h>

namespace p2 {
  template<typename T>
  inline T min(const T &lhs, const T &rhs) {
    return (lhs > rhs ? rhs : lhs);
  }

  template<typename T>
  inline T max(const T &lhs, const T &rhs) {
    return (lhs > rhs ? lhs : rhs);
  }
}

const char *strchr(const char *str, char c);

// Functions used by the compiler sometimes for optimization
extern "C" void *memset(void *dest, int value, size_t len);

#endif // !PEOS2_UTILS_H

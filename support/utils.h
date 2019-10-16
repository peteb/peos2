// -*- c++ -*-

#ifndef PEOS2_UTILS_H
#define PEOS2_UTILS_H

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

#endif // !PEOS2_UTILS_H

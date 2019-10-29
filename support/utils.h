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

  template<typename T> struct remove_reference      {typedef T type; };
  template<typename T> struct remove_reference<T&>  {typedef T type; };
  template<typename T> struct remove_reference<T&&> {typedef T type; };

  template<class T>
  T&& forward(typename remove_reference<T>::type& t) noexcept {
    return static_cast<T&&>(t);
  }

  template <class T>
  T&& forward(typename remove_reference<T>::type&& t) noexcept {
    return static_cast<T&&>(t);
  }
}

const char *strchr(const char *str, char c);

// Functions used by the compiler sometimes for optimization
extern "C" void *memset(void *dest, int value, size_t len);

#if __STDC_HOSTED__ == 0
// Placement new operator overloads
inline void *operator new(size_t, void *p)     throw() {return p;}
inline void *operator new[](size_t, void *p)   throw() {return p;}
inline void  operator delete  (void *, void *) throw() {};
inline void  operator delete[](void *, void *) throw() {};
#endif

#endif // !PEOS2_UTILS_H

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

  template<typename T>
  inline T clamp(const T &val, const T &low, const T &high) {
    return max(min(val, high), low);
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

  class non_copyable {
  public:
    non_copyable() = default;
    ~non_copyable() = default;

  private:
    non_copyable(const non_copyable &) = delete;
    non_copyable &operator =(const non_copyable &) = delete;
  };
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

char *strchr(char *str, char c);
char *strnchr(char *str, char c, size_t len);
size_t strlen(const char *str);
int strncmp(const char *s1, const char *s2, size_t len);
char *strncpy(char *dest, const char *src, size_t len);

// Functions used by the compiler sometimes for optimization
extern "C" void *memset(void *dest, int value, size_t len);
extern "C" void *memcpy(void *dest, const void *src, size_t length);

#if __STDC_HOSTED__ == 0
// Placement new operator overloads
inline void *operator new(size_t, void *p)     noexcept {return p;}
inline void *operator new[](size_t, void *p)   noexcept {return p;}
inline void  operator delete  (void *, void *) noexcept {};
inline void  operator delete[](void *, void *) noexcept {};
#endif

#endif // !PEOS2_UTILS_H

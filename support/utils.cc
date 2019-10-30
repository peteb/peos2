#include "utils.h"

extern "C" void *memset(void *dest, int value, size_t len) {
  char *ptr = (char *)dest;
  while (len--) {
    *ptr++ = value;
  }
  return dest;
}

const char *strchr(const char *str, char c) {
  while (*str != c) {
    if (!*str++) {
      return 0;
    }
  }

  return str;
}

size_t strlen(const char *str) {
  size_t length = 0;
  while (*str++) {
    ++length;
  }
  return length;
}

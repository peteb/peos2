#include "utils.h"

extern "C" void *memset(void *dest, int value, size_t len)
{
  // TODO: optimize
  char *ptr = (char *)dest;
  while (len--) {
    *ptr++ = value;
  }
  return dest;
}

extern "C" void *memcpy(void *dest, const void *src, size_t length)
{
  // TODO: optimize aligned copy etc
  char *d = (char *)dest;
  const char *s = (const char *)src;

  for (size_t i = 0; i < length; ++i) {
    *d++ = *s++;
  }

  return dest;
}

const char *strchr(const char *str, char c)
{
  while (*str != c) {
    if (!*str++) {
      return 0;
    }
  }

  return str;
}

size_t strlen(const char *str)
{
  size_t length = 0;
  while (*str++) {
    ++length;
  }
  return length;
}

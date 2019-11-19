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

char *strchr(char *str, char c)
{
  while (*str != c) {
    if (!*str++) {
      return 0;
    }
  }

  return str;
}

char *strnchr(char *str, char c, size_t len)
{
  while (*str != c) {
    if (!*str++ || !len--) {
      return 0;
    }
  }

  return len ? str : nullptr;
}

size_t strlen(const char *str)
{
  size_t length = 0;
  while (*str++) {
    ++length;
  }
  return length;
}

int strncmp(const char *s1, const char *s2, size_t len)
{
  while (len--) {
    if (*s1++ != *s2++)
      return *(unsigned char *)(s1 - 1) - *(unsigned char *)(s2 - 1);
  }

  return 0;
}

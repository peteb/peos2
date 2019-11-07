#include "tar.h"
#include "support/string.h"

uint32_t tar_parse_octal(const char *str, size_t length)
{
  p2::string<13> s(str, length);
  uint32_t ret = 0;
  for (size_t i = 0; i < length - 1; ++i) {
    ret = ret * 8 + (s[i] - '0');
  }
  return ret;
}

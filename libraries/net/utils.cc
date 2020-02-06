#include <stdint.h>

#include "utils.h"

char debug_out_buffer[512];

uint64_t sum_words(const char *data, size_t length)
{
  uint64_t sum = 0;

  // Warning: possibly non-aligned reads, but works on my CPU...
  const uint32_t *ptr32 = (const uint32_t *)data;

  while (length >= 4) {
    sum += *ptr32++;
    length -= 4;
  }

  // Might be leftovers
  const uint16_t *ptr16 = (const uint16_t *)ptr32;

  if (length >= 2) {
    sum += *ptr16++;
    length -= 2;
  }

  // Might be even more leftovers (which should be padded)
  if (length > 0) {
    uint8_t byte = *(const uint8_t *)ptr16;
    sum += htons(byte << 8);
    --length;
  }

  assert(length == 0);

  return sum;
}

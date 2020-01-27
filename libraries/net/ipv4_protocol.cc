#include <support/assert.h>

#include "ipv4_protocol.h"
#include "utils.h"

uint16_t ipv4_checksum(const ipv4_header &header)
{
  assert(header.checksum == 0);

  auto sum = sum_words(reinterpret_cast<const char *>(&header), sizeof(header));
  while (sum > 0xFFFF)
    sum = (sum >> 16) + (sum & 0xFFFF);

  return ~(uint16_t)sum;
}

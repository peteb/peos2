#include <stdint.h>

#include "utils.h"

using namespace p2;

char debug_out_buffer[512];

p2::string<32> hwaddr_str(const uint8_t *octets)
{
  return p2::format<32>("%02x:%02x:%02x:%02x:%02x:%02x",
                        (uint32_t)octets[0], (uint32_t)octets[1], (uint32_t)octets[2], (uint32_t)octets[3],
                        (uint32_t)octets[4], (uint32_t)octets[5]).str();
}

p2::string<16> ipaddr_str(uint32_t octets)
{
  return p2::format<16>("%d.%d.%d.%d",
                        (octets >> 24) & 0xFF,
                        (octets >> 16) & 0xFF,
                        (octets >> 8) & 0xFF,
                        (octets >> 0) & 0xFF).str();
}

uint32_t parse_ipaddr(const char *str)
{
  // Supports only the simple format 123.255.1.0.128

  const char *pos = str;
  uint32_t ipaddr = 0;
  int current_octet = 0;

  while (true) {
    if (isdigit(*pos)) {
      current_octet = (current_octet * 10 + (*pos++ - '0'));
    }
    else if (*pos == '.' || *pos == '\0') {
      assert(current_octet >= 0 && current_octet <= 255);
      // TODO: signal parse fail rather than panic
      ipaddr = ((ipaddr << 8) | (current_octet & 0xFF));
      current_octet = 0;

      if (*pos == '\0')
        break;
      else
        ++pos;
    }
  }

  return ipaddr;
}

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

uint16_t ipv4_checksum(const char *data, size_t length, const char *data2, size_t length2)
{
  uint32_t sum = 0;
  sum += sum_words(data, length);
  sum += sum_words(data2, length2);

  while (sum > 0xFFFF)
    sum = (sum >> 16) + (sum & 0xFFFF);

  return ~(uint16_t)sum;
}

#include <support/utils.h>

#include "tcp_proto.h"
#include "utils.h"

uint16_t tcp_checksum(uint32_t src_addr, uint32_t dest_addr, uint8_t proto, const char *data1, size_t length1, const char *data2, size_t length2)
{
  struct pseudo_header {
    uint32_t src_addr;
    uint32_t dest_addr;
    uint8_t zeroes;
    uint8_t proto;
    uint16_t length;
  } __attribute__((packed));

  pseudo_header hdr;
  hdr.src_addr = htonl(src_addr);
  hdr.dest_addr = htonl(dest_addr);
  hdr.zeroes = 0;
  hdr.proto = proto;
  hdr.length = htons(length1 + length2);

  uint64_t sum = 0;
  sum += sum_words((char *)&hdr, sizeof(hdr));
  sum += sum_words(data1, length1);
  sum += sum_words(data2, length2);

  while (sum > 0xFFFF)
    sum = (sum >> 16) + (sum & 0xFFFF);

  return ~(uint16_t)sum;
}

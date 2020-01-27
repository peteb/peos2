// -*- c++ -*-

#ifndef PEOS2_NET_IPV4_PROTOCOL_H
#define PEOS2_NET_IPV4_PROTOCOL_H

#include <stdint.h>

#define FLAGS_DF  0x01
#define FLAGS_MF  0x02

struct ipv4_header {
  uint8_t  ihl:4;
  uint8_t  version:4;
  uint8_t  ecn_dscp;
  uint16_t total_len;
  uint16_t id;
  uint16_t frag_ofs;
  uint8_t  ttl;
  uint8_t  protocol;
  uint16_t checksum;
  uint32_t src_addr;
  uint32_t dest_addr;
} __attribute__((packed));

uint16_t ipv4_checksum(const ipv4_header &header);

#endif // !PEOS2_NET_IPV4_PROTOCOL_H

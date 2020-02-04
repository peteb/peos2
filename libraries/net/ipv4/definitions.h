#pragma once

#include <stdint.h>

namespace net::ipv4 {

  using address = uint32_t;

  enum flags {
    FLAGS_DF = 0x01,
    FLAGS_MF = 0x02
  };

  enum proto : uint8_t {
    PROTO_ICMP =  1,
    PROTO_TCP  =  6,
    PROTO_UDP  = 17
  };

  struct header {
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

  struct datagram_metadata {
    net::ipv4::address src_addr;
    net::ipv4::address dest_addr;
  };

}

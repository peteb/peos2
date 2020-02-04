#pragma once

#include <stdint.h>

#include "ipv4/definitions.h"

namespace net::ipv4 {
  struct datagram_metadata;
}

namespace net::tcp {
  enum flags : uint16_t {
    FIN = 0x0001,
    SYN = 0x0002,
    RST = 0x0004,
    PSH = 0x0008,
    ACK = 0x0010,
    URG = 0x0020,
    ECE = 0x0040,
    CWR = 0x0080,
    NS  = 0x0100
  };

  using sequence_number = uint32_t;

  struct header {
    uint16_t src_port;
    uint16_t dest_port;
    sequence_number seq_nbr;
    uint32_t ack_nbr;
    uint16_t flags_hdrsz;
    uint16_t wndsz;
    uint16_t checksum;
    uint16_t urgptr;
  } __attribute__((packed));

  struct segment_metadata {
    header *tcp_header;
    const net::ipv4::datagram_metadata *datagram;
    uint16_t flags;
  };

  struct endpoint {
    net::ipv4::address ipaddr;
    uint16_t port;

    static const endpoint wildcard;
  };
}

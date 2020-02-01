#pragma once

#include <stdint.h>

namespace net::ethernet {
  struct address {
    operator void *() {return octets; }
    operator const void *() const {return octets; }
    uint8_t operator [](size_t idx) const {return octets[idx]; }

    static const address wildcard, broadcast;

    uint8_t octets[6];
  };

  enum ether_type : uint16_t {
    ET_IPV4 = 0x0800,
    ET_ARP  = 0x0806,
    ET_IPV6 = 0x86DD,
    ET_FLOW = 0x8808
  };

  struct header {
    address mac_dest;
    address mac_src;
    uint16_t type;
  } __attribute__((packed));

  struct frame_metadata {
    address *mac_dest;
    address *mac_src;
  };
}
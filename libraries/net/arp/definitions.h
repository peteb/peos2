#pragma once

#include <stdint.h>

#include "ethernet/definitions.h"
#include "ipv4/definitions.h"

namespace net::arp {
  struct header {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    net::ethernet::address sha;
    net::ipv4::address spa;
    net::ethernet::address tha;
    net::ipv4::address tpa;
  } __attribute__((packed));

  enum op : uint16_t {
    OP_REQUEST = 1,
    OP_REPLY   = 2
  };
}

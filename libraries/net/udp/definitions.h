#pragma once

#include <stdint.h>

namespace net::udp {
  struct header {
    uint16_t src_port, dest_port;
    uint16_t length, checksum;
  } __attribute__((packed));
}

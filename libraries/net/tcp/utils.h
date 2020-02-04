#pragma once

#include <stddef.h>
#include "net/ipv4/definitions.h"

namespace net::tcp {
  // Calculate checksum that can be used in tcphdr::checksum
  uint16_t checksum(net::ipv4::address src_addr,
                    net::ipv4::address dest_addr,
                    net::ipv4::proto protocol,
                    const char *data1,
                    size_t length1,
                    const char *data2,
                    size_t length2);
}
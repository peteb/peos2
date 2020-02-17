#pragma once

#include <support/string.h>
#include "ipv4/definitions.h"

namespace net::ipv4 {
  p2::string<16> ipaddr_str(address octets);
  address parse_ipaddr(const char *str);
  uint16_t checksum(const net::ipv4::header &header, const char *options = nullptr, size_t options_length = 0);
}

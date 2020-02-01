#pragma once

#include <support/string.h>

#include "ethernet/definitions.h"

namespace net::ethernet {
  p2::string<32> hwaddr_str(const address &octets);
  p2::string<32> ether_type_str(ether_type type);
}

#include <support/format.h>

#include "ethernet/utils.h"

namespace net::ethernet {

p2::string<32> hwaddr_str(const address &octets)
{
  return p2::format<32>("%02x:%02x:%02x:%02x:%02x:%02x",
                        (uint32_t)octets[0], (uint32_t)octets[1], (uint32_t)octets[2], (uint32_t)octets[3],
                        (uint32_t)octets[4], (uint32_t)octets[5]).str();
}

p2::string<32> ether_type_str(ether_type type)
{
  switch (type) {
  case ET_ARP:
    return "ARP";

  case ET_FLOW:
    return "FLOW";

  case ET_IPV4:
    return "IPv4";

  case ET_IPV6:
    return "IPv6";

  default:
    return p2::format<32>("%04x", type).str();
  }
}
}

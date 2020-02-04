#include <support/utils.h>
#include "ethernet/definitions.h"

namespace {
  net::ethernet::address create_wildcard()
  {
    net::ethernet::address addr;
    uint8_t octets[6] = {0, 0, 0, 0, 0, 0};
    memcpy(&addr, octets, sizeof(addr));
    return addr;
  }

  net::ethernet::address create_broadcast()
  {
    net::ethernet::address addr;
    uint8_t octets[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(&addr, octets, sizeof(addr));
    return addr;
  }
}

namespace net::ethernet {
  const address address::wildcard = create_wildcard();
  const address address::broadcast = create_broadcast();
}

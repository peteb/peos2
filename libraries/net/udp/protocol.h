#pragma once

#include <stddef.h>

namespace net::ipv4 {
  class datagram_metadata;
}

namespace net::udp {
  class protocol {
  public:
    void on_receive(const net::ipv4::datagram_metadata &datagram, const char *data, size_t length);
  };
}
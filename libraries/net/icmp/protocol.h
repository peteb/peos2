#pragma once

#include <stddef.h>

namespace net::ipv4 {
  class datagram_metadata;
}

namespace net::icmp {
  class protocol {
  public:
    virtual void on_receive(const net::ipv4::datagram_metadata &datagram, const char *data, size_t length) = 0;
  };

  class protocol_impl : public protocol {
  public:
    void on_receive(const net::ipv4::datagram_metadata &, const char *, size_t) final {}
  };
}

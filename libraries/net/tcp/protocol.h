#pragma once

#include <stddef.h>
#include "tcp/connection_table.h"

namespace net {
  class protocol_stack;
}

namespace net::ipv4 {
  struct datagram_metadata;
}

namespace net::tcp {
  class protocol {
  public:
    protocol(net::protocol_stack &protocols);

    void on_receive(const net::ipv4::datagram_metadata &metadata, const char *data, size_t length);
    void listen(const net::tcp::endpoint &local);

  private:
    net::protocol_stack &_protocols;
    connection_table _connections;
  };
}

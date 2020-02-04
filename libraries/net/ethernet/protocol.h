#pragma once

#include <stddef.h>
#include "ethernet/definitions.h"

namespace net {
  class protocol_stack;
}

namespace net::ethernet {
  // Main ethernet state for a stack
  class protocol {
  public:
    protocol(protocol_stack &protocols);

    void configure(const address &hwaddr);
    void on_receive(const char *data, size_t length);
    int send(ether_type type, const address &destination, const char *data, size_t length);

    const address &hwaddr() const {return _hwaddr; }

  private:
    protocol_stack &_protocols;
    address _hwaddr;
  };

}

#pragma once

#include "ethernet/definitions.h"
#include "ipv4/definitions.h"

namespace net {
  class protocol_stack;
}

namespace net::arp {

  // Main ARP state for a stack
  class protocol {
  public:
    protocol(protocol_stack &protocols) : _protocols(protocols) {}

    void on_receive(const net::ethernet::frame_metadata &metadata, const char *data, size_t length);
    int send(int op, net::ipv4::address tpa, const net::ethernet::address &tha, const net::ethernet::address &next_hop);

  private:
    protocol_stack &_protocols;
  };

}

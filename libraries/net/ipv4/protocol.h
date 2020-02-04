#pragma once

#include <stddef.h>
#include <support/flip_buffer.h>

#include "ipv4/definitions.h"
#include "ethernet/definitions.h"

namespace net {
  class protocol_stack;
}

namespace net::ethernet {
  class frame_metadata;
}

namespace net::ipv4 {
  class protocol {
  public:
    protocol(protocol_stack &protocols);

    void configure(address local, address netmask, address default_gateway);
    void on_receive(const net::ethernet::frame_metadata &metadata, const char *data, size_t length);
    bool send(net::ipv4::proto protocol, net::ipv4::address destination, const char *data, size_t length);
    void send(net::ipv4::proto protocol, net::ipv4::address destination, net::ethernet::address next_hop, const char *data, size_t length);
    void tick(uint32_t delta_ms);

    address local_address() const {return _local; }

  private:
    void send_single_datagram(net::ipv4::proto protocol,
                              net::ipv4::address src_addr,
                              net::ipv4::address dest_addr,
                              net::ethernet::address next_hop,
                              const char *data,
                              size_t length);

  private:
    address _local, _netmask, _default_gateway;
    p2::flip_buffer<10240> _arp_wait_buffer;  // Holding area for packets awaiting ARP
    uint16_t _next_datagram_id = 0;

    protocol_stack &_protocols;
  };
}

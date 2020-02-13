#pragma once

#include <stddef.h>
#include <support/flip_buffer.h>
#include <support/unordered_map.h>

#include "ipv4/definitions.h"
#include "ipv4/reassembly_buffer.h"
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
    virtual void configure(address local, address netmask, address default_gateway) = 0;
    virtual void tick(uint32_t delta_ms) = 0;
    virtual address local_address() const = 0;

    virtual void on_receive(const net::ethernet::frame_metadata &metadata, const char *data, size_t length) = 0;
    virtual bool send(net::ipv4::proto protocol, net::ipv4::address destination, const char *data, size_t length) = 0;
    virtual void send(net::ipv4::proto protocol, net::ipv4::address destination, net::ethernet::address next_hop, const char *data, size_t length) = 0;
  };

  class protocol_impl : public protocol {
  public:
    protocol_impl(protocol_stack &protocols);

    void configure(address local, address netmask, address default_gateway) final;
    void on_receive(const net::ethernet::frame_metadata &metadata, const char *data, size_t length) final;
    bool send(net::ipv4::proto protocol, net::ipv4::address destination, const char *data, size_t length) final;
    void send(net::ipv4::proto protocol, net::ipv4::address destination, net::ethernet::address next_hop, const char *data, size_t length) final;
    void tick(uint32_t delta_ms) final;

    address local_address() const final {return _local; }

  private:
    void send_single_datagram(net::ipv4::proto protocol,
                              net::ipv4::address src_addr,
                              net::ipv4::address dest_addr,
                              net::ethernet::address next_hop,
                              const char *data,
                              size_t length);
    bool is_recipient(net::ipv4::address dest_addr) const;

  private:
    address _local, _netmask, _default_gateway;

    p2::flip_buffer<10240> _arp_wait_buffer;  // Holding area for packets awaiting ARP
    uint16_t _next_datagram_id = 0;
    p2::unordered_map<reassembly_buffer_identifier, reassembly_buffer, 10> _reassembly_buffers;

    protocol_stack &_protocols;
  };
}

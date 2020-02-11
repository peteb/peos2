#pragma once

#include <support/assert.h>
#include "protocol_stack.h"
#include "ipv4/protocol.h"

#include <queue>
#include <vector>

namespace net {
  class protocol_stack_mock : public protocol_stack {
  public:
    net::device &device() final               {assert(_device); return *_device; }
    net::ethernet::protocol &ethernet() final {assert(_ethernet); return *_ethernet; }
    net::arp::protocol &arp() final           {assert(_arp); return *_arp; }
    net::ipv4::protocol &ipv4() final         {assert(_ipv4); return *_ipv4; }
    net::tcp::protocol &tcp() final           {assert(_tcp); return *_tcp; }
    net::udp::protocol &udp() final           {assert(_udp); return *_udp; }

  void tick(uint32_t) final {}

    net::device *_device = nullptr;
    net::ethernet::protocol *_ethernet = nullptr;
    net::arp::protocol *_arp = nullptr;
    net::ipv4::protocol *_ipv4 = nullptr;
    net::tcp::protocol *_tcp = nullptr;
    net::udp::protocol *_udp = nullptr;
  };
}

namespace net::ipv4 {
  class protocol_mock : public protocol {
  public:
    void configure(address, address, address) override {}
    void tick(uint32_t) override {}
    address local_address() const override {return {}; }

    void on_receive(const net::ethernet::frame_metadata &, const char *, size_t) override {}
    bool send(net::ipv4::proto, net::ipv4::address, const char *, size_t) override {return true; }
    void send(net::ipv4::proto, net::ipv4::address, net::ethernet::address, const char *, size_t) override {}
  };
}

namespace net::udp {
  class protocol_mock : public protocol {
  public:
    struct on_receive_invocation {
      net::ipv4::datagram_metadata datagram;
      std::vector<char> data;
    };

    void on_receive(const net::ipv4::datagram_metadata &datagram, const char *data, size_t length)
    {
      on_receive_invocations.push({datagram, std::move(std::vector<char>(data, data + length))});
    }

    std::queue<on_receive_invocation> on_receive_invocations;
  };
}
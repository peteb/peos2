#pragma once

#include "ethernet/protocol.h"
#include "arp/protocol.h"
#include "ipv4/protocol.h"
#include "tcp/protocol.h"
#include "udp/protocol.h"

namespace net {

class device {
public:
  virtual int send(const char *data, size_t length) = 0;
};

class protocol_stack {
public:
  virtual net::device &device() = 0;
  virtual net::ethernet::protocol &ethernet() = 0;
  virtual net::arp::protocol &arp() = 0;
  virtual net::ipv4::protocol &ipv4() = 0;
  virtual net::tcp::protocol &tcp() = 0;
  virtual net::udp::protocol &udp() = 0;

  virtual void tick(uint32_t delta_ms) = 0;
};

// A protocol is part of a protocol stack and is connected to other protocols;
// Ethernet <-> IP <-> TCP etc.
// Typically, one protocol stack is instantiated per ethernet device.
class protocol_stack_impl : public protocol_stack {
public:
  protocol_stack_impl(net::device *device) : _device(device), _ethernet(*this), _arp(*this), _ipv4(*this), _tcp(*this) {}

  net::device &device() final {return *_device; }
  net::ethernet::protocol &ethernet() final {return _ethernet; }
  net::arp::protocol &arp() final {return _arp; }
  net::ipv4::protocol &ipv4() final {return _ipv4; }
  net::tcp::protocol &tcp() final {return _tcp; }
  net::udp::protocol &udp() final {return _udp; }

  void tick(uint32_t delta_ms) final
  {
    arp().tick(delta_ms);
  }

private:
  net::device *_device;
  net::ethernet::protocol _ethernet;
  net::arp::protocol_impl _arp;
  net::ipv4::protocol_impl _ipv4;
  net::tcp::protocol _tcp;
  net::udp::protocol_impl _udp;
};

}
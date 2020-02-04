#pragma once

#include "ethernet/protocol.h"
#include "arp/protocol.h"
#include "ipv4/protocol.h"
#include "tcp/protocol.h"

namespace net {

class device {
public:
  virtual int send(const char *data, size_t length) = 0;
};

// A protocol is part of a protocol stack and is connected to other protocols;
// Ethernet <-> IP <-> TCP etc.
// Typically, one protocol stack is instantiated per ethernet device.
class protocol_stack {
public:
  protocol_stack(net::device *device) : _device(device), _ethernet(*this), _arp(*this), _ipv4(*this), _tcp(*this) {}

  net::device &device() {return *_device; }
  net::ethernet::protocol &ethernet() {return _ethernet; }
  net::arp::protocol &arp() {return _arp; }
  net::ipv4::protocol *ipv4() {return &_ipv4; }
  net::tcp::protocol &tcp() {return _tcp; }

  void tick(uint32_t delta_ms)
  {
    arp().tick(delta_ms);
  }

private:
  net::device *_device;
  net::ethernet::protocol _ethernet;
  net::arp::protocol _arp;
  net::ipv4::protocol _ipv4;
  net::tcp::protocol _tcp;
};


}
#pragma once

#include "ipv4/definitions.h"

namespace net::ipv4 {

class protocol {
public:
  void configure(address local, address netmask, address default_gateway)
  {
    _local = local;
    _netmask = netmask;
    _default_gateway = default_gateway;
  }

  address local_address() const {return _local; }

private:
  address _local, _netmask, _default_gateway;
};

}

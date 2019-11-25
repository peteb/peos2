// -*- c++ -*-

#ifndef NET_ARP_PROBE_H
#define NET_ARP_PROBE_H

#include <stdint.h>
#include "utils.h"

static void send_ipv4_request(int fd, uint32_t ipaddr);

class probe_ipv4 {
public:
  probe_ipv4(int fd, uint32_t ipaddr)
    : _interface(fd), _ipaddr(ipaddr)
  {
    reset();
    send_ipv4_request(_interface, _ipaddr);
  }

  bool tick(int dt)
  {
    _resend_timer -= dt;

    if (_resend_timer <= 0) {
      if (_resends++ > 6)
        return false;

      _last_delta *= 2;
      _resend_timer = _last_delta;
      send_ipv4_request(_interface, _ipaddr);
    }

    return true;
  }

  void reset()
  {
    _resend_timer = _last_delta = 40;
  }

  bool probingFor(uint32_t addr)
  {
    return _ipaddr == addr;
  }

private:
  int _interface;
  uint32_t _ipaddr;
  int _last_delta = 0;
  int _resend_timer = 0;
  int _resends = 0;
};

#endif // !NET_ARP_PROBE_H

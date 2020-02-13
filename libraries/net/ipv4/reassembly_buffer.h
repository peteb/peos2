#pragma once

#include <support/frag_buffer.h>
#include "ipv4/definitions.h"

namespace net::ipv4 {
  struct reassembly_buffer_identifier {
    bool operator ==(const reassembly_buffer_identifier &other) const
    {
      return dest == other.dest && source == other.source && id == other.id && protocol == other.protocol;
    }

    net::ipv4::address dest, source;
    uint16_t id;
    uint8_t protocol;
  };

  struct reassembly_buffer {
    int ttl = 20'000;
    bool received_last = false;
    p2::frag_buffer<10'000> data;  // 10 000 is arbitrarily chosen. Eventually we might get a dynamic limit.
  };
}
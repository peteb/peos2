#pragma once

#include <support/queue.h>
#include <support/unordered_map.h>
#include <support/optional.h>

#include "ethernet/definitions.h"
#include "ipv4/definitions.h"
#include "retryable.h"

namespace net {
  class protocol_stack;
}

namespace net::arp {
  using ipv4_lookup_result = const net::ethernet::address *;
  using probe = retryable<ipv4_lookup_result &>;

  // ARP mapping table (IPv4 -> MAC). Might be replaced with an LRU cache in the future, but should be good enough for now
  // as we're mostly going to talk to the default gateway.
  class mapping_table {
  public:
    void                          insert(net::ipv4::address ipaddr, net::ethernet::address hwaddr);
    const net::ethernet::address *fetch(net::ipv4::address ipaddr) const;

  private:
    struct cache_entry {
      net::ipv4::address ipaddr;
      net::ethernet::address hwaddr;
    };

    p2::queue<cache_entry, 64> _entries;
  };

  // Main ARP state for a stack
  class protocol {
  public:
    protocol(protocol_stack &protocols) : _protocols(protocols) {}

    void on_receive(const net::ethernet::frame_metadata &metadata, const char *data, size_t length);
    int  send(int op, net::ipv4::address tpa, const net::ethernet::address &tha, const net::ethernet::address &next_hop);
    void tick(uint32_t delta_time);

    ipv4_lookup_result fetch_cached(net::ipv4::address ipaddr) const;
    void fetch_network(net::ipv4::address ipaddr, probe::await_fun callback);

  private:
    void write_cache_entry(net::ipv4::address ipaddr, net::ethernet::address hwaddr);

    protocol_stack &_protocols;
    mapping_table _mappings;
    p2::unordered_map<net::ipv4::address, probe, 32> _active_probes;
  };
}

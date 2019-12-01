// -*- c++ -*-

#ifndef NET_IPV4_H
#define NET_IPV4_H

#include <stdint.h>
#include <stddef.h>

#define PROTO_ICMP   1
#define PROTO_TCP    6
#define PROTO_UDP   17

// Information about an IPv4 datagram
struct ipv4_dgram {
  uint8_t ttl;
  uint32_t src_addr, dest_addr;
  uint8_t proto;
};

// IPv4 needs to know which IP address we're sending from to populate
// fields and to know which remotes are on the same subnet.
// The implementation will immediately check the ip addresses using ARP
void ipv4_configure(int interface,
                    uint32_t ipaddr,
                    uint32_t netmask,
                    uint32_t gwaddr);

// Forward a PDU from a lower layer
void ipv4_recv(int interface,
               struct eth_frame *frame,
               const char *data,
               size_t length);

// Send a datagram
size_t ipv4_send(int interface,
                 const ipv4_dgram &ipv4,
                 const char *data,
                 size_t length);

// Advances the IPv4 world by @dt ticks, used for timeouts, etc
void ipv4_tick(int dt);

// Fetch the local IP address
int ipv4_local_address(uint32_t *addr);

#endif // !NET_IPV4_H

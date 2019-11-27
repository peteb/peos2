// -*- c++ -*-

#ifndef NET_IPV4_H
#define NET_IPV4_H

#include <stdint.h>
#include <stddef.h>

struct ipv4_dgram {
  uint8_t ttl;
  uint32_t src_addr, dest_addr;
};

void ipv4_configure(int interface, uint32_t ipaddr, uint32_t netmask, uint32_t gwaddr);
void ipv4_recv(int interface, struct eth_frame *frame, const char *data, size_t length);
size_t ipv4_send(int interface, const ipv4_dgram &ipv4, const char *data, size_t length);

void ipv4_tick(int dt);
int  ipv4_local_address(uint32_t *addr);

#endif // !NET_IPV4_H

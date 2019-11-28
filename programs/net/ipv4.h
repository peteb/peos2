// -*- c++ -*-

#ifndef NET_IPV4_H
#define NET_IPV4_H

#include <stdint.h>
#include <stddef.h>

#define PROTO_ICMP   1
#define PROTO_TCP    6
#define PROTO_UDP   17

struct ipv4_dgram {
  uint8_t ttl;
  uint32_t src_addr, dest_addr;
  uint8_t proto;
};

void ipv4_configure(int interface, uint32_t ipaddr, uint32_t netmask, uint32_t gwaddr);
void ipv4_recv(int interface, struct eth_frame *frame, const char *data, size_t length);
size_t ipv4_send(int interface, const ipv4_dgram &ipv4, const char *data, size_t length);
uint64_t sum_words(const char *data, size_t length);
uint16_t ipv4_checksum(const char *data, size_t length, const char *data2, size_t length2);

void ipv4_tick(int dt);
int  ipv4_local_address(uint32_t *addr);

#endif // !NET_IPV4_H

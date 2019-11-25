// -*- c++ -*-

#ifndef NET_IPV4_H
#define NET_IPV4_H

#include <stdint.h>
#include <stddef.h>

void ipv4_configure(int interface, uint32_t ipaddr, uint32_t netmask, uint32_t gwaddr);
void ipv4_recv(int interface, struct eth_frame *frame, const char *data, size_t length);
int  ipv4_local_address(uint32_t *addr);

#endif // !NET_IPV4_H

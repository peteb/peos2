// -*- c++ -*-

#ifndef NET_ARP_H
#define NET_ARP_H

#include <stddef.h>

struct eth_frame;

void arp_tick(int ticks);
void arp_recv(int eth_fd, eth_frame *frame, const char *data, size_t length);
int arp_lookup_ipv4(int fd, uint32_t ipaddr, uint8_t *hwaddr);

#endif // !NET_ARP_H

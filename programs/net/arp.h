// -*- c++ -*-

#ifndef NET_ARP_H
#define NET_ARP_H

#include <stddef.h>
#include <support/function.h>
#include "retryable.h"

struct eth_frame;

enum probe_result {
  PROBE_REPLY,
  PROBE_TIMEOUT
};

void arp_tick(int ticks);
void arp_recv(int eth_fd, eth_frame *frame, const char *data, size_t length);
int  arp_request_lookup_ipv4(int fd, uint32_t ipaddr, retryable<probe_result>::await_fun callback);
int  arp_cache_lookup_ipv4(int fd, uint32_t ipaddr, uint8_t *hwaddr);

#endif // !NET_ARP_H

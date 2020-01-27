// -*- c++ -*-

#ifndef NET_ETHERNET_H
#define NET_ETHERNET_H

#include <stdint.h>

#define ET_IPV4 0x0800
#define ET_ARP  0x0806
#define ET_IPV6 0x86DD
#define ET_FLOW 0x8808

struct eth_frame {
  const uint8_t *dest;
  const uint8_t *src;
  uint16_t type;
};

void eth_configure(int interface, uint8_t *local_hwaddr);
void eth_on_receive(int interface, const char *data, size_t size);
int eth_send(int fd, eth_frame *frame, const char *data, size_t size);
void eth_hwaddr(int fd, uint8_t *octets);

#endif // !NET_ETHERNHET_H

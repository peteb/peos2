// -*- c++ -*-

#ifndef NET_ETHERNET_H
#define NET_ETHERNET_H

#define ET_IPV4 0x0800
#define ET_ARP  0x0806
#define ET_IPV6 0x86DD
#define ET_FLOW 0x8808

void eth_run(int fd);

#endif // !NET_ETHERNHET_H

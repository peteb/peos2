// -*- c++ -*-

#ifndef NET_UDP_H
#define NET_UDP_H

#include <stddef.h>

#include "ethernet.h"
#include "ipv4.h"

void udp_recv(int interface, eth_frame *frame, ipv4_info *datagram, const char *data, size_t length);

#endif // !NET_UDP_H

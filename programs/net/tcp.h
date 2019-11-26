// -*- c++ -*-

#ifndef NET_TCP_H
#define NET_TCP_H

#include <stddef.h>
#include "ipv4.h"
#include "ethernet.h"

void tcp_recv(int interface, eth_frame *frame, ipv4_dgram *datagram, const char *data, size_t length);

#endif // !NET_TCP_H

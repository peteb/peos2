// -*- c++ -*-

#ifndef NET_TCP_H
#define NET_TCP_H

#include <stddef.h>

#include "ipv4.h"
#include "ethernet.h"

// Intialize the TCP subsystem
void tcp_init(int interface);

// Send data to the TCP subsystem (from a lower layer)
void tcp_recv(int interface,
              eth_frame *frame,
              ipv4_info *datagram,
              const char *data,
              size_t length);

void tcp_tick(int dt);

#endif // !NET_TCP_H

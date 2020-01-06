// -*- c++ -*-

#ifndef NET_TCP_H
#define NET_TCP_H

#include <stddef.h>
#include <stdint.h>

#include "ipv4.h"
#include "ethernet.h"

using tcp_connection_handle = void *;

struct tcp_connection_listeners {
  void (*on_receive)(tcp_connection_handle connection, const char *data, size_t length);
};

// Intialize the TCP subsystem
void tcp_init(int interface);

tcp_connection_handle tcp_listen(uint16_t port, const tcp_connection_listeners &listeners);

// Send data to the TCP subsystem (from a lower layer)
void tcp_recv(int interface,
              eth_frame *frame,
              ipv4_info *datagram,
              const char *data,
              size_t length);

void tcp_send(tcp_connection_handle connection, const char *data, size_t length);
void tcp_close(tcp_connection_handle connection);

void tcp_tick(int dt);

#endif // !NET_TCP_H

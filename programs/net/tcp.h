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
              ipv4_dgram *datagram,
              const char *data,
              size_t length);

// Calculate checksum that can be used in tcphdr::checksum
uint16_t tcp_checksum(uint32_t src_addr,
                      uint32_t dest_addr,
                      uint8_t proto,
                      const char *data1,
                      size_t length1,
                      const char *data2,
                      size_t length2);

#endif // !NET_TCP_H

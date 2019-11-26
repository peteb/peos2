// -*- c++ -*-

#ifndef NET_TCP_CONNECTION_H
#define NET_TCP_CONNECTION_H

#include <stdint.h>
#include "net/ipv4.h"
#include "net/ethernet.h"

#define FIN 0x0001
#define SYN 0x0002
#define RST 0x0004
#define PSH 0x0008
#define ACK 0x0010
#define URG 0x0020
#define ECE 0x0040
#define CWR 0x0080
#define NS  0x0100

struct tcp_header {
  uint16_t src_port;
  uint16_t dest_port;
  uint32_t seq_nbr;
  uint32_t ack_nbr;
  uint16_t flags_hdrsz;
  uint16_t wndsz;
  uint16_t checksum;
  uint16_t urgptr;
} __attribute__((packed));

struct tcp_endpoint {
  uint32_t ipaddr;
  uint16_t port;
};

struct tcp_segment {
  eth_frame *frame;
  ipv4_dgram *datagram;
  tcp_header *tcphdr;
};

class tcp_connection_state {
public:
  virtual void recv(const tcp_segment &segment) const {(void)segment; }

  static const tcp_connection_state *LISTEN;
};


class tcp_connection {
public:
  tcp_connection(const tcp_endpoint &remote, const tcp_endpoint &local, const tcp_connection_state *state)
    : _remote(remote),
      _local(local),
      _state(state)
  {
  }

  // compare - checks whether the @remote and @local matches the
  // connection and how well they do. If the connection has
  // non-wildcard fields that don't match, then the function returns
  // -1. Otherwise, it returns the number of non-wildcard fields that
  // match exactly.
  int compare(const tcp_endpoint &remote, const tcp_endpoint &local);

  // recv - handles a freshly arrived datagram from IPv4
  void recv(const tcp_segment &segment);

private:
  tcp_endpoint _remote, _local;
  const tcp_connection_state *_state;
};

#endif // !NET_TCP_CONNECTION_H

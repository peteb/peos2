// -*- c++ -*-

#ifndef NET_TCP_CONNECTION_H
#define NET_TCP_CONNECTION_H

#include <stdint.h>

#include "tcp_proto.h"
#include "tcp_recv_queue.h"
#include "tcp_connection_state.h"

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

  void rx_enqueue(const tcp_segment &segment);

private:
  tcp_endpoint _remote, _local;
  const tcp_connection_state *_state;

  tcp_recv_queue _rx_queue;
};

#endif // !NET_TCP_CONNECTION_H

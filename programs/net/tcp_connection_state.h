// -*- c++ -*-

#ifndef NET_TCP_CONNECTION_STATE_H
#define NET_TCP_CONNECTION_STATE_H

#include "tcp_recv_queue.h"
#include "tcp_proto.h"

class tcp_connection;

class tcp_connection_state {
public:
  virtual const char *name() const =0;

  // Called when a segment has just arrived -- might be a duplicate,
  // in wrong order, etc.
  virtual void early_recv(tcp_connection &connection,
                          const tcp_segment &segment) const =0;

  // Segment received in correct order, no duplicates
  virtual void sequenced_recv(tcp_connection &connection,
                              const tcp_recv_segment &segment,
                              const char *data,
                              size_t length) const =0;

  static const tcp_connection_state *LISTEN, *SYN_RCVD, *ESTABLISHED;
};

#endif // !NET_TCP_CONNECTION_STATE_H

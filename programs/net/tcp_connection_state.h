// -*- c++ -*-

#ifndef NET_TCP_CONNECTION_STATE_H
#define NET_TCP_CONNECTION_STATE_H

#include "tcp_recv_queue.h"
#include "tcp_proto.h"

class tcp_connection;

class tcp_connection_state {
public:
  virtual void early_recv(tcp_connection &connection, const tcp_segment &segment) const =0;
  virtual void recv(tcp_connection &connection, const tcp_recv_segment &segment) const =0;

  static const tcp_connection_state *LISTEN, *SYN_SENT;
};

#endif // !NET_TCP_CONNECTION_STATE_H

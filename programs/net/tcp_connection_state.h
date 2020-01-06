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
                          const tcp_segment &segment) const
  {
    (void)connection;
    (void)segment;
  }

  // Segment received in correct order, no duplicates
  virtual void sequenced_recv(tcp_connection &connection,
                              const tcp_recv_segment &segment,
                              const char *data,
                              size_t length) const
  {
    (void)connection;
    (void)segment;
    (void)data;
    (void)length;
  }

  // TODO: better name
  virtual void remote_consumed_all(tcp_connection &connection) const
  {
    (void)connection;
  }

  virtual void active_close(tcp_connection &connection) const
  {
    (void)connection;
  }

  static const tcp_connection_state *LISTEN, *SYN_RCVD, *ESTABLISHED,
    *LAST_ACK, *FIN_WAIT_1, *FIN_WAIT_2, *CLOSING;
};

#endif // !NET_TCP_CONNECTION_STATE_H

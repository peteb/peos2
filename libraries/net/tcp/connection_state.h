// -*- c++ -*-

#pragma once

#include "tcp_recv_queue.h"

namespace net::tcp {
  class connection;
  class segment_metadata;
}

namespace net::tcp {
  class connection_state {
  public:
    virtual const char *name() const = 0;

    // Called when a segment has just arrived -- might be a duplicate,
    // in wrong order, etc.
    virtual void early_recv(connection &connection,
                            const segment_metadata &metadata,
                            const char *data,
                            size_t length) const
    {
      (void)connection;
      (void)metadata;
      (void)data;
      (void)length;
    }

    // Segment received in correct order, no duplicates
    virtual void sequenced_recv(connection &connection,
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
    virtual void remote_consumed_all(connection &connection) const
    {
      (void)connection;
    }

    virtual void active_close(connection &connection) const
    {
      (void)connection;
    }

    static const connection_state *LISTEN, *SYN_RCVD, *ESTABLISHED,
      *LAST_ACK, *FIN_WAIT_1, *FIN_WAIT_2, *CLOSING;
  };

}

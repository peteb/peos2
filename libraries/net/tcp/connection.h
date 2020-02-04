// -*- c++ -*-

#pragma once

#include <stdint.h>

#include "tcp/definitions.h"

#include "tcp_recv_queue.h"
#include "tcp_send_queue.h"
#include "tcp_connection_state.h"

namespace net::ipv4 {
  class protocol;
}

namespace net::tcp {
  class connection_state;
  class connection_table;
}

namespace net::tcp {
  // Connects a local endpoint (which might or might not be a wildcard)
  // to a remote endpoint. The connection object handles retransmission,
  // acking, reassembly, etc, but it's controlled by the current
  // connection state.
  class connection {
  public:
    connection(net::ipv4::protocol &ipv4,
               net::tcp::connection_table &table,
               const endpoint &remote,
               const endpoint &local,
               const connection_state *state)
      : _ipv4(ipv4),
        _connection_table(table),
        _remote(remote),
        _local(local),
        _state(state)
    {
    }

    // Checks how well @remote and @local match the connection. If the
    // connection has non-wildcard fields that don't match, then the
    // function returns -1. Otherwise, it returns the number of
    // non-wildcard fields that match exactly.
    int compare(const endpoint &remote,
                const endpoint &local);

    void mark_for_destruction();
    void on_receive(const segment_metadata &metadata, const char *data, size_t length);

    // Clear the transmit queue and set its sequence number
    void reset_tx(sequence_number outgoing_seqnbr);

    // Clear the receive queue and set the next sequence number it expects
    void reset_rx(sequence_number incoming_seqnbr);

    // Write a segment to the receive queue so that it'll get sorted and
    // ack'd correctly. Length must be at least 1, you cannot sequence
    // an empty segment, thus, you cannot sequence an empty ACK.
    // Consuming a sequenced segment without writing something to the tx
    // buffer will trigger an empty ack to be sent.
    void sequence(const segment_metadata &metadata,
                  const char *data,
                  size_t length);

    // Write a segment to the transmit queue so that it'll be sent off
    // and re-sent on timeout
    void transmit(const tcp_send_segment &segment,
                  const char *data,
                  size_t length,
                  size_t send_length);

    void transmit_phantom(const tcp_send_segment &segment)
    {
      static char phantom[1] = {'!'};
      transmit(segment, phantom, 1, 0);
    }

    void transition(const connection_state *new_state);

    connection_table &table() { return _connection_table; }

    // Advances internal timers and executes timeouts
    void tick(int dt);

    void step();

    void close();

    uint16_t handle;

  private:
    void send(uint16_t flags, net::tcp::sequence_number seqnbr,
              const char *data,
              size_t length);

    net::ipv4::protocol &_ipv4;
    connection_table &_connection_table;

    endpoint _remote, _local;

    const connection_state *_state;

    tcp_recv_queue _rx_queue;
    tcp_send_queue _tx_queue;

    sequence_number _next_outgoing_seqnbr = 0;
  };


}

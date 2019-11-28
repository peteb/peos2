// -*- c++ -*-

#ifndef NET_TCP_CONNECTION_H
#define NET_TCP_CONNECTION_H

#include <stdint.h>

#include "tcp_proto.h"
#include "tcp_recv_queue.h"
#include "tcp_send_queue.h"
#include "tcp_connection_state.h"

class tcp_connection_table;

// Connects a local endpoint (which might or might not be a wildcard)
// to a remote endpoint. The connection object handles retransmission,
// acking, reassembly, etc, but it's controlled by the current
// connection state.
class tcp_connection {
public:
  tcp_connection(tcp_connection_table *connection_table,
                 const tcp_endpoint &remote,
                 const tcp_endpoint &local,
                 const tcp_connection_state *state)
    : _connection_table(connection_table),
      _remote(remote),
      _local(local),
      _state(state)
  {
  }

  // Checks how well @remote and @local match the connection. If the
  // connection has non-wildcard fields that don't match, then the
  // function returns -1. Otherwise, it returns the number of
  // non-wildcard fields that match exactly.
  int compare(const tcp_endpoint &remote, const tcp_endpoint &local);

  // Handle a freshly arrived datagram from IPv4
  void recv(const tcp_segment &segment);

  // Clear the transmit queue and set its sequence number
  void reset_tx(tcp_seqnbr outgoing_seqnbr);

  // Clear the receive queue and set the next sequence number it expects
  void reset_rx(tcp_seqnbr incoming_seqnbr);

  // Write a segment to the receive queue so that it'll get sorted and
  // ack'd correctly
  void rx_enqueue(const tcp_segment &segment);

  // Write a segment to the transmit queue so that it'll be sent off
  // and re-sent on timeout
  void tx_enqueue(const tcp_send_segment &segment,
                  const char *data,
                  size_t length);

  void transition(const tcp_connection_state *new_state);

  tcp_connection_table &connection_table() { return *_connection_table; }

private:
  void send(const tcp_send_segment &segment, const char *data, size_t length);

  tcp_connection_table *_connection_table;
  tcp_endpoint _remote, _local;
  const tcp_connection_state *_state;

  tcp_recv_queue _rx_queue;
  tcp_send_queue _tx_queue;
};

#endif // !NET_TCP_CONNECTION_H

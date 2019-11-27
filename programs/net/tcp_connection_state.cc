#include "net/tcp_connection.h"
#include "net/tcp_connection_state.h"
#include "utils.h"

// LISTEN state
static const class : public tcp_connection_state {
  void early_recv(tcp_connection &conn, const tcp_segment &segment) const override {
    log(tcp, "LISTEN: rx segment length=%d", segment.payload_size);

    // TODO: create a new connection with our own made-up seq nbr
    // TODO: tx_enqueue a syn back (which will use our seq nbr). The late stage tx will add ACK
    conn.rx_enqueue(segment);
  }

  void recv(tcp_connection &connection, const tcp_recv_segment &segment) const override {
    (void)segment;
    (void)connection;
    log(tcp, "LISTEN: rx ordered segment, shouldn't happen");
  }

} listen;

const tcp_connection_state *tcp_connection_state::LISTEN = &listen;

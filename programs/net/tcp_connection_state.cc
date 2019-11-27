#include "net/tcp_connection.h"
#include "net/tcp_connection_state.h"
#include "utils.h"

// LISTEN state
static const class : public tcp_connection_state {
  void early_recv(tcp_connection &conn, const tcp_segment &segment) const override {
    (void)segment;
    log(tcp, "rx segment in LISTEN (length: %d)", segment.payload_size);
    conn.rx_enqueue(segment);
  }

  void recv(tcp_connection &connection, const tcp_recv_segment &segment) const override {
    (void)segment;
    (void)connection;
    log(tcp, "rx ordered segment in LISTEN");
  }

} listen;
const tcp_connection_state *tcp_connection_state::LISTEN = &listen;

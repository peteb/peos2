#include "net/tcp_connection.h"
#include "net/tcp_connection_state.h"
#include "net/tcp_connection_table.h"
#include "utils.h"

#include "ipv4.h"

// LISTEN state
static const class : public tcp_connection_state {
  void early_recv(tcp_connection &conn, const tcp_segment &segment) const override {
    log(tcp, "LISTEN: rx segment length=%d", segment.payload_size);

    tcp_connection_table &conntab = conn.connection_table();

    auto conn_idx = conntab.create_connection({segment.datagram->src_addr, segment.tcphdr->src_port},
                                              {segment.datagram->dest_addr, segment.tcphdr->dest_port},
                                              tcp_connection_state::SYN_SENT);

    tcp_connection &client_conn = conntab[conn_idx];
    tcp_seqnbr isn = 0xDEADBEEF;
    client_conn.reset_rx(segment.tcphdr->seq_nbr + 1);
    client_conn.reset_tx(isn);

    char phantom_data[1] = {0};
    tcp_send_segment response;
    response.flags = SYN;
    response.seqnbr = isn;
    client_conn.tx_enqueue(response, phantom_data, 1);
  }

  void recv(tcp_connection &connection, const tcp_recv_segment &segment) const override {
    (void)segment;
    (void)connection;
    log(tcp, "LISTEN: rx ordered segment, shouldn't happen");
  }

} listen;

const tcp_connection_state *tcp_connection_state::LISTEN = &listen;

// SYN-SENT state
static const class : public tcp_connection_state {
  void early_recv(tcp_connection &conn, const tcp_segment &segment) const override {
    (void)conn;
    (void)segment;
    log(tcp, "SYN-SENT: rx early recv");
  }

  void recv(tcp_connection &connection, const tcp_recv_segment &segment) const override {
    (void)segment;
    (void)connection;
    log(tcp, "SYN-SENT: rx ordered segment, shouldn't happen");
  }

} syn_sent;

const tcp_connection_state *tcp_connection_state::SYN_SENT = &syn_sent;

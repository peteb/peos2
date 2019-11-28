#include "net/tcp_connection.h"
#include "net/tcp_connection_state.h"
#include "net/tcp_connection_table.h"
#include "utils.h"

#include "ipv4.h"

// LISTEN - server is waiting for SYN segments
static const class : public tcp_connection_state {
  void early_recv(tcp_connection &conn, const tcp_segment &segment) const override {
    // TODO: check that the segment has the SYN bit set

    log(tcp, "LISTEN: rx segment length=%d", segment.payload_size);

    tcp_connection_table &conntab = conn.connection_table();

    auto conn_idx = conntab.create_connection({segment.datagram->src_addr, segment.tcphdr->src_port},
                                              {segment.datagram->dest_addr, segment.tcphdr->dest_port},
                                              tcp_connection_state::SYN_RCVD);

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

  void recv(tcp_connection &connection, const tcp_recv_segment &segment, const char *data, size_t length) const override {
    (void)segment;
    (void)connection;
    (void)data;
    (void)length;
    log(tcp, "LISTEN: rx ordered segment, shouldn't happen");
  }

} listen;

const tcp_connection_state *tcp_connection_state::LISTEN = &listen;


// SYN_RCVD - when server has received a SYN request and sent back a
// SYN (which automatically gets an ACK appended)
static const class : public tcp_connection_state {
  void early_recv(tcp_connection &conn, const tcp_segment &segment) const override {
    log(tcp, "SYN-RCVD: rx early recv");

    // TODO: extra check on sequence numbers?

    if (segment.flags == ACK) {
      conn.transition(tcp_connection_state::ESTABLISHED);
    }
  }

  void recv(tcp_connection &connection, const tcp_recv_segment &segment, const char *data, size_t length) const override {
    (void)data;
    (void)length;
    (void)segment;
    (void)connection;
    log(tcp, "SYN-RCVD: rx ordered segment, shouldn't happen");
  }

} syn_rcvd;

const tcp_connection_state *tcp_connection_state::SYN_RCVD = &syn_rcvd;



// ESTABLISHED - handshake is done and we've got synchronized sequence
// numbers; sending data both ways
static const class : public tcp_connection_state {
  void early_recv(tcp_connection &conn, const tcp_segment &segment) const override {
    (void)conn;
    (void)segment;
    log(tcp, "ESTABLISHED: rx early recv");
    // TODO: check if this is a FIN or something else
    conn.rx_enqueue(segment);
  }

  void recv(tcp_connection &connection, const tcp_recv_segment &segment, const char *data, size_t length) const override {
    (void)segment;
    (void)connection;
    char buffer[256];
    memcpy(buffer, data, p2::min(length, sizeof(buffer)));
    buffer[p2::min(length, sizeof(buffer) - 1)] = 0;
    log(tcp, "ESTABLISHED: rx sequenced segment %d bytes: %s", length, buffer);
  }

} established;

const tcp_connection_state *tcp_connection_state::ESTABLISHED = &established;

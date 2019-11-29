#include "net/tcp_connection.h"
#include "net/tcp_connection_state.h"
#include "net/tcp_connection_table.h"
#include "utils.h"

#include "ipv4.h"

// LISTEN - server is waiting for SYN segments
static const class : public tcp_connection_state {
  const char *name() const
  {
    return "LISTEN";
  }

  void early_recv(tcp_connection &conn,
                  const tcp_segment &segment) const override
  {
    // TODO: check that the segment has the SYN bit set
    log(tcp, "LISTEN: rx segment length=%d", segment.payload_size);

    tcp_connection_table &conntab = conn.connection_table();
    auto conn_idx = conntab.create_connection({segment.datagram->src_addr, segment.tcphdr->src_port},
                                              {segment.datagram->dest_addr, segment.tcphdr->dest_port},
                                              tcp_connection_state::SYN_RCVD);

    // Create a new connection where we'll sequence this SYN (and thus send the ACK)
    tcp_connection &client_conn = conntab[conn_idx];
    tcp_seqnbr isn = 0xDEADBEEF;
    client_conn.reset_rx(segment.tcphdr->seq_nbr);
    client_conn.reset_tx(isn);

    // Even if we don't receive or send the implicit length of 1 for
    // SYNs and FINs, we need to increase the cursor of the rx buffer
    // to allow acking etc
    char phantom[1] = {'!'};
    client_conn.sequence(segment, phantom, 1);
  }

  void sequenced_recv(tcp_connection &connection,
                      const tcp_recv_segment &segment,
                      const char *data,
                      size_t length) const override
  {
    (void)segment;
    (void)connection;
    (void)data;
    (void)length;
    log(tcp, "LISTEN: rx sequenced segment, shouldn't happen");
  }

} listen;

const tcp_connection_state *tcp_connection_state::LISTEN = &listen;


// SYN_RCVD - when server has received a SYN request and sent back a
// SYN (which automatically gets an ACK appended)
static const class : public tcp_connection_state {
  const char *name() const
  {
    return "SYN-RCVD";
  }

  void early_recv(tcp_connection &conn,
                  const tcp_segment &segment) const override
  {
    log(tcp, "SYN-RCVD: rx early recv");

    // TODO: extra check on sequence numbers?

    if (segment.flags == ACK) {
      // TODO: Is it possible to append data to the handshake ACK? In
      // that case, we should sequence the ACK payload
      conn.transition(tcp_connection_state::ESTABLISHED);
    }
  }

  void sequenced_recv(tcp_connection &connection,
                      const tcp_recv_segment &segment,
                      const char *data,
                      size_t length) const override
  {
    (void)data;
    (void)length;
    (void)segment;
    (void)connection;
    log(tcp, "SYN-RCVD: rx sequenced segment of size %d", length);

    // TODO: verify that this is a sequenced SYN
    tcp_send_segment response;
    char phantom[1] = {'!'};
    response.flags = SYN;
    connection.transmit(response, phantom, 1, 0);
  }

} syn_rcvd;

const tcp_connection_state *tcp_connection_state::SYN_RCVD = &syn_rcvd;



// ESTABLISHED - handshake is done and we've got synchronized sequence
// numbers; sending data both ways
static const class : public tcp_connection_state {
  const char *name() const
  {
    return "ESTABLISHED";
  }

  void early_recv(tcp_connection &conn,
                  const tcp_segment &segment) const override
  {
    (void)conn;
    (void)segment;
    log(tcp, "ESTABLISHED: rx early recv");
    // TODO: check if this is a FIN or something else
    conn.sequence(segment, segment.payload, segment.payload_size);
  }

  void sequenced_recv(tcp_connection &connection,
                      const tcp_recv_segment &segment,
                      const char *data, size_t length) const override
  {
    (void)segment;
    (void)connection;
    char buffer[256];
    memcpy(buffer, data, p2::min(length, sizeof(buffer)));
    buffer[p2::min(length, sizeof(buffer) - 1)] = 0;
    log(tcp, "ESTABLISHED: rx sequenced segment %d bytes: %s", length, buffer);
  }

} established;

const tcp_connection_state *tcp_connection_state::ESTABLISHED = &established;

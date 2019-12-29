#include "net/tcp_connection.h"
#include "net/tcp_connection_state.h"
#include "net/tcp_connection_table.h"
#include "utils.h"

#include "ipv4.h"

static char phantom[1] = {'!'};

// LISTEN - server is waiting for SYN segments
static const class : public tcp_connection_state {
  const char *name() const
  {
    return "LISTEN";
  }

  void early_recv(tcp_connection &conn,
                  const tcp_segment &segment) const override
  {
    if (!(segment.flags & SYN)) {
      log(tcp, "LISTEN: received segment without SYN, dropping");
      // TODO: RST
      return;
    }

    // Create a new connection for this pair of endpoints
    // TODO: what if this connection is the most specific pair of endpoints already?
    tcp_connection_table &conntab = conn.connection_table();
    auto conn_idx = conntab.create_connection({segment.datagram->src_addr, segment.tcphdr->src_port},
                                              {segment.datagram->dest_addr, segment.tcphdr->dest_port},
                                              tcp_connection_state::SYN_RCVD);

    // Sequence this SYN, and thus send the ACK, in the new connection
    tcp_connection &client_conn = conntab[conn_idx];
    tcp_seqnbr isn = 0xDEADBEEF;  // TODO: generate
    client_conn.reset_rx(segment.tcphdr->seq_nbr);
    client_conn.reset_tx(isn);

    // Even if we don't receive or send the implicit length of 1 for
    // SYNs and FINs, we need to increase the cursor of the rx buffer
    // to allow acking etc
    client_conn.sequence(segment, phantom, 1);
  }

  void sequenced_recv(tcp_connection &,
                      const tcp_recv_segment &,
                      const char *,
                      size_t) const override
  {
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
    // TODO: extra check on sequence numbers?

    if (segment.flags & ACK) {
      // TODO: Is it possible to append data to the handshake ACK? In
      // that case, we should sequence the ACK payload
      conn.transition(tcp_connection_state::ESTABLISHED);
    }
    else {
      log(tcp, "SYN-RCVD: early_recv of non-ACK, flags=%04x, dropping", segment.flags);
    }
  }

  void sequenced_recv(tcp_connection &connection,
                      const tcp_recv_segment &segment,
                      const char *data,
                      size_t length) const override
  {
    if (!(segment.flags & SYN)) {
      log(tcp, "SYN-RCVD: sequenced a segment without SYN set");
      return;
    }

    (void)data;
    log(tcp, "SYN-RCVD: flags %04x", segment.flags);
    log(tcp, "SYN-RCVD: rx sequenced SYN segment of size %d, sending back", length);

    // TODO: verify that this is a sequenced SYN
    tcp_send_segment response;
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

  void early_recv(tcp_connection &conn, const tcp_segment &segment) const override
  {
    if ((segment.flags & FIN) && segment.payload_size == 0) {
      // We need to sequence FINs with a phantom byte so that they
      // increase the sequence number and can be ACKd
      conn.sequence(segment, phantom, 1);
    }
    else {
      conn.sequence(segment, segment.payload, segment.payload_size);
    }
  }

  void sequenced_recv(tcp_connection &conn,
                      const tcp_recv_segment &segment,
                      const char *data,
                      size_t length) const override
  {
    if (segment.flags & FIN) {
      // Remote wants to shut the connection down, so ACK that segment
      // (will be done automatically) and send them a FIN of our
      // own. Because we don't have an application to wait for, we're
      // bypassing the CLOSE-WAIT state.  TODO: implement CLOSE-WAIT
      // on server side

      tcp_send_segment response;
      response.flags = FIN;
      conn.transmit(response, phantom, 1, 0);
      conn.transition(tcp_connection_state::LAST_ACK);
      return;
    }

    char buffer[256];
    memcpy(buffer, data, p2::min(length, sizeof(buffer)));
    buffer[p2::min(length, sizeof(buffer) - 1)] = 0;
    log(tcp, "ESTABLISHED: rx seq segment %d bytes: %s", length, buffer);

    if (length > 0) {
      // Send some dummy data
      const char *message = "HTTP/1.1 200 OK\r\n"
        "Server: peos2\r\n"
        "Content-Length: 16\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: Closed\r\n"
        "\r\n"
        "Handled by peos2";

      (void)message;

      tcp_send_segment http_segment;
      http_segment.flags = PSH;
      conn.transmit(http_segment, message, strlen(message), strlen(message));

      // Close the connection
      tcp_send_segment fin_segment;
      fin_segment.flags = FIN;
      conn.transmit(fin_segment, phantom, 1, 0);
      conn.transition(tcp_connection_state::FIN_WAIT_1);
    }

  }

} established;

const tcp_connection_state *tcp_connection_state::ESTABLISHED = &established;


// LAST_ACK
static const class : public tcp_connection_state {
  const char *name() const
  {
    return "LAST-ACK";
  }

  void remote_consumed_all(tcp_connection &conn) const
  {
    log(tcp, "LAST-ACK: remote consumed all our messages, closing down connection...");
    conn.mark_for_destruction();
  }

} last_ack;

const tcp_connection_state *tcp_connection_state::LAST_ACK = &last_ack;


// FIN_WAIT_1
static const class : public tcp_connection_state {
  const char *name() const
  {
    return "FIN-WAIT-1";
  }

  void early_recv(tcp_connection &conn, const tcp_segment &segment) const override
  {
    if ((segment.flags & FIN) && segment.payload_size == 0) {
      // We need to sequence FINs with a phantom byte so that they
      // increase the sequence number and can be ACKd

      // TODO: if we don't implement the "recv_sequenced" function, we
      // shouldn't consume from the rx queue. This would forward any
      // FINs from the other side to the FIN-WAIT-2 state
      log(tcp, "FIN-WAIT-1: received a FIN -- the other endpoint is shutting down at the same time");
      conn.sequence(segment, phantom, 1);
      conn.transition(tcp_connection_state::CLOSING);
      return;
    }

    log(tcp, "FIN-WAIT-1: received non-FIN message, dropping...");
    // TODO: what if we receive data here, should we do anything with it?
  }

  void remote_consumed_all(tcp_connection &conn) const
  {
    // Our FIN has been ACK'd
    log(tcp, "FIN-WAIT-1: remote ACK'd our FIN, waiting for their FIN...");
    conn.transition(tcp_connection_state::FIN_WAIT_2);
  }

} fin_wait_1;

const tcp_connection_state *tcp_connection_state::FIN_WAIT_1 = &fin_wait_1;



// CLOSING - when the endpoints are simultaneously closing down
static const class : public tcp_connection_state {
  const char *name() const
  {
    return "FIN-CLOSING";
  }

  void remote_consumed_all(tcp_connection &conn) const
  {
    log(tcp, "CLOSING: remote ACK'd all messages");
    conn.mark_for_destruction();
  }
} closing;

const tcp_connection_state *tcp_connection_state::CLOSING = &closing;



// FIN_WAIT_2
static const class : public tcp_connection_state {
  const char *name() const
  {
    return "FIN-WAIT-2";
  }

  void early_recv(tcp_connection &conn, const tcp_segment &segment) const override
  {
    if ((segment.flags & FIN) && segment.payload_size == 0) {
      // We need to sequence FINs with a phantom byte so that they
      // increase the sequence number and can be ACKd
      conn.sequence(segment, phantom, 1);
      return;
    }

    log(tcp, "FIN-WAIT-2: received non-FIN message, dropping");
    // TODO: what if we receive data here, should we do anything with it?
  }

  void sequenced_recv(tcp_connection &conn,
                      const tcp_recv_segment &segment,
                      const char *data,
                      size_t length) const override
  {
    (void)data;
    (void)length;
    if (segment.flags & FIN) {
      log(tcp, "FIN-WAIT-2: received remote FIN, shutting down...");
      conn.mark_for_destruction();  // TODO: use the CLOSED state instead
    }
    else {
      log(tcp, "FIN-WAIT-2: received non-FIN segment, dropping");
    }
  }
} fin_wait_2;

const tcp_connection_state *tcp_connection_state::FIN_WAIT_2 = &fin_wait_2;

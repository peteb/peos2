#include <support/logging.h>

#include "tcp/connection_state.h"
#include "tcp/connection_table.h"
#include "utils.h"
#include "../utils.h"

namespace net::tcp {

static char phantom[1] = {'!'};

// LISTEN - server is waiting for SYN segments
static const class : public connection_state {
  const char *name() const
  {
    return "LISTEN";
  }

  void early_recv(connection &conn,
                  const segment_metadata &metadata,
                  const char *data,
                  size_t length) const override
  {
    if (!(metadata.flags & SYN)) {
      log_info("LISTEN: received segment without SYN, dropping");
      // TODO: RST
      return;
    }

    (void)data;
    (void)length;

    // Create a new conneciton for this pair of endpoints
    // TODO: what if this connection is the most specific pair of endpoints already?
    connection_table &conntab = conn.table();
    auto conn_idx = conntab.create_connection({metadata.datagram->src_addr, metadata.tcp_header->src_port},
                                              {metadata.datagram->dest_addr, metadata.tcp_header->dest_port},
                                              connection_state::SYN_RCVD);

    // Sequence this SYN, and thus send the ACK, in the new connection
    connection &client_conn = conntab[conn_idx];
    net::tcp::sequence_number isn = 0xDEADBEEF;  // TODO: generate
    client_conn.reset_rx(metadata.tcp_header->seq_nbr);
    client_conn.reset_tx(isn);

    // Even if we don't receive or send the implicit length of 1 for
    // SYNs and FINs, we need to increase the cursor of the rx buffer
    // to allow acking etc
    client_conn.sequence(metadata, phantom, 1);
  }

  void sequenced_recv(connection &,
                      const tcp_recv_segment &,
                      const char *,
                      size_t) const override
  {
    log_info("LISTEN: rx sequenced segment, shouldn't happen");
  }

} listen;

const connection_state *connection_state::LISTEN = &listen;


// SYN_RCVD - when server has received a SYN request and sent back a
// SYN (which automatically gets an ACK appended)
static const class : public connection_state {
  const char *name() const
  {
    return "SYN-RCVD";
  }

  void early_recv(connection &conn,
                  const segment_metadata &metadata,
                  const char *data,
                  size_t length) const override
  {
    // TODO: extra check on sequence numbers?
    (void)data;
    (void)length;

    if (metadata.flags & ACK) {
      // TODO: Is it possible to append data to the handshake ACK? In
      // that case, we should sequence the ACK payload
      conn.transition(connection_state::ESTABLISHED);
    }
    else {
      log_info("SYN-RCVD: early_recv of non-ACK, flags=%04x, dropping", metadata.flags);
    }
  }

  void sequenced_recv(connection &connection,
                      const tcp_recv_segment &segment,
                      const char *data,
                      size_t length) const override
  {
    if (!(segment.flags & SYN)) {
      log_info("SYN-RCVD: sequenced a segment without SYN set");
      return;
    }

    (void)data;
    (void)length;

    log_debug("SYN-RCVD: rx sequenced SYN segment of size %d, sending back", length);

    // TODO: verify that this is a sequenced SYN
    tcp_send_segment response;
    response.flags = SYN;
    connection.transmit(response, phantom, 1, 0);
  }

} syn_rcvd;

const connection_state *connection_state::SYN_RCVD = &syn_rcvd;



// ESTABLISHED - handshake is done and we've got synchronized sequence
// numbers; sending data both ways
static const class : public connection_state {
  const char *name() const
  {
    return "ESTABLISHED";
  }

  void early_recv(connection &conn, const segment_metadata &metadata, const char *data, size_t length) const override
  {
    if ((metadata.flags & FIN) && length == 0) {
      // We need to sequence FINs with a phantom byte so that they
      // increase the sequence number and can be ACKd
      conn.sequence(metadata, phantom, 1);
    }
    else {
      conn.sequence(metadata, data, length);
    }
  }

  void sequenced_recv(connection &connection,
                      const tcp_recv_segment &segment,
                      const char *data,
                      size_t length) const override
  {
    (void)data;
    (void)length;

    if (segment.flags & FIN) {
      // Remote wants to shut the connection down, so ACK that segment
      // (will be done automatically) and send them a FIN of our
      // own. Because we don't have an application to wait for, we're
      // bypassing the CLOSE-WAIT state.  TODO: implement CLOSE-WAIT
      // on server side

      tcp_send_segment response;
      response.flags = FIN;
      connection.transmit(response, phantom, 1, 0);
      connection.transition(connection_state::LAST_ACK);
      return;
    }

    connection.on_full_receive(data, length);
  }

  void active_close(connection &connection) const
  {
    tcp_send_segment fin_segment;
    fin_segment.flags = FIN;
    connection.transmit_phantom(fin_segment);
    connection.transition(connection_state::FIN_WAIT_1);
  }

} established;

const connection_state *connection_state::ESTABLISHED = &established;


// LAST_ACK
static const class : public connection_state {
  const char *name() const
  {
    return "LAST-ACK";
  }

  void remote_consumed_all(connection &conn) const
  {
    log_debug("LAST-ACK: remote consumed all our messages, closing down connection...");
    conn.mark_for_destruction();
  }

} last_ack;

const connection_state *connection_state::LAST_ACK = &last_ack;


// FIN_WAIT_1
static const class : public connection_state {
  const char *name() const
  {
    return "FIN-WAIT-1";
  }

  void early_recv(connection &connection,
                  const segment_metadata &metadata,
                  const char *data,
                  size_t length) const final
  {
    (void)data;

    if ((metadata.flags & FIN) && length == 0) {
      // We need to sequence FINs with a phantom byte so that they
      // increase the sequence number and can be ACKd

      // TODO: if we don't implement the "recv_sequenced" function, we
      // shouldn't consume from the rx queue. This would forward any
      // FINs from the other side to the FIN-WAIT-2 state
      log_debug("FIN-WAIT-1: received a FIN -- the other endpoint is shutting down at the same time");
      connection.sequence(metadata, phantom, 1);
      connection.transition(connection_state::CLOSING);
      return;
    }

    log_debug("FIN-WAIT-1: received non-FIN message, dropping...");
    // TODO: what if we receive data here, should we do anything with it?
  }

  void remote_consumed_all(connection &conn) const
  {
    // Our FIN has been ACK'd
    log_debug("FIN-WAIT-1: remote ACK'd our FIN, waiting for their FIN...");
    conn.transition(connection_state::FIN_WAIT_2);
  }

} fin_wait_1;

const connection_state *connection_state::FIN_WAIT_1 = &fin_wait_1;



// CLOSING - when the endpoints are simultaneously closing down
static const class : public connection_state {
  const char *name() const
  {
    return "FIN-CLOSING";
  }

  void remote_consumed_all(connection &conn) const
  {
    log_debug("CLOSING: remote ACK'd all messages");
    conn.mark_for_destruction();
  }
} closing;

const connection_state *connection_state::CLOSING = &closing;



// FIN_WAIT_2
static const class : public connection_state {
  const char *name() const
  {
    return "FIN-WAIT-2";
  }

  void early_recv(connection &conn,
                            const segment_metadata &metadata,
                            const char *data,
                            size_t length) const override
  {
    (void)data;

    if ((metadata.flags & FIN) && length == 0) {
      // We need to sequence FINs with a phantom byte so that they
      // increase the sequence number and can be ACKd
      conn.sequence(metadata, phantom, 1);
      return;
    }

    log_debug("FIN-WAIT-2: received non-FIN message, dropping");
    // TODO: what if we receive data here, should we do anything with it?
  }

  void sequenced_recv(connection &conn,
                      const tcp_recv_segment &segment,
                      const char *data,
                      size_t length) const override
  {
    (void)data;
    (void)length;
    if (segment.flags & FIN) {
      log_debug("FIN-WAIT-2: received remote FIN, shutting down...");
      conn.mark_for_destruction();  // TODO: use the CLOSED state instead
    }
    else {
      log_debug("FIN-WAIT-2: received non-FIN segment, dropping");
    }
  }
} fin_wait_2;

const connection_state *connection_state::FIN_WAIT_2 = &fin_wait_2;

}

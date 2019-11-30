#include <support/assert.h>

#include "tcp_connection_table.h"
#include "tcp_connection.h"
#include "utils.h"
#include "tcp.h"

namespace {
  int count_specific_matches(const tcp_endpoint &filter, const tcp_endpoint &endpoint)
  {
    int matching_fields = 0;

    if (filter.ipaddr != 0) {
      if (endpoint.ipaddr == filter.ipaddr)
        ++matching_fields;
      else
        return -1;
    }

    if (filter.port != 0) {
      if (endpoint.port == filter.port)
        ++matching_fields;
      else
        return -1;
    }

    return matching_fields;
  }
}


int tcp_connection::compare(const tcp_endpoint &remote, const tcp_endpoint &local)
{
  int remote_match = count_specific_matches(_remote, remote);
  int local_match = count_specific_matches(_local, local);

  if (remote_match >= 0 && local_match >= 0)
    return remote_match + local_match;
  else
    return -1;
}

void tcp_connection::recv(const tcp_segment &segment)
{
  assert(_state);

  _state->early_recv(*this, segment);

  // Automatically use the ack seqnbr from remote to stop our
  // retransmission of packets
  if (segment.flags & ACK) {
    _tx_queue.ack(segment.tcphdr->ack_nbr);

    if (segment.tcphdr->ack_nbr >= _tx_queue.write_cursor()) {
      // Remote has ACK'd everything we've sent
      _state->remote_consumed_all(*this);
    }
  }

  // The sender's segment will be ack'd by the tx queue automatically
  // on transmission or after a timeout
  step();
}

void tcp_connection::mark_for_destruction()
{
  _connection_table->finish_connection(handle);
}

void tcp_connection::step()
{
  static char buffer[20 * 1024];
  // TODO: review this buffer size
  // TODO: change static when this is multithreaded

  const bool received_sequenced = _rx_queue.has_readable();

  while (_rx_queue.has_readable()) {
    tcp_recv_segment sequenced_segment;

    if (!_rx_queue.read_one_segment(&sequenced_segment, buffer, sizeof(buffer))) {
      // This is weird, has_readable returned true...!
      log(tcp, "failed to read one segment");
      return;
    }

    _state->sequenced_recv(*this, sequenced_segment, buffer, sequenced_segment.length);
  }

  if (received_sequenced && !_tx_queue.has_readable()) {
    // Nothing was added to the tx queue though we know we've received
    // a sequenced message, so how will this message be ack'd to
    // remote? It won't! So send an empty message -- this will carry
    // the ACK.
    // We're bypassing the TX queue because it might have more data
    // than what the window allows, which would lead to the ACK
    // getting queued

    log(tcp_connection, "sending empty ack with seqnbr=% 12d", _next_outgoing_seqnbr);
    tcp_send_segment segment;
    segment.flags = 0;
    segment.seqnbr = _next_outgoing_seqnbr;
    send(segment, nullptr, 0);
  }

  // Send outgoing messages if we have any
  while (_tx_queue.has_readable()) {
    log(tcp_connection, "sending outstanding tx");
    tcp_send_segment seg;
    size_t bytes_read = _tx_queue.read_one_segment(&seg, buffer, sizeof(buffer));
    send(seg, buffer, bytes_read);
  }
}

void tcp_connection::sequence(const tcp_segment &segment, const char *data, size_t length)
{
  tcp_recv_segment segment_;
  segment_.flags = segment.tcphdr->flags_hdrsz & 0x1FF;
  segment_.seqnbr = segment.tcphdr->seq_nbr;

  _rx_queue.insert(segment_, data, length);
  // TODO: handle result? What if the queue is full?
}

void tcp_connection::transmit(const tcp_send_segment &segment, const char *data, size_t length, size_t send_length)
{
  tcp_send_segment segment_(segment);
  segment_.seqnbr = _tx_queue.write_cursor();
  _tx_queue.write_back(segment_, data, length, send_length);
}

void tcp_connection::reset_tx(tcp_seqnbr outgoing_seqnbr)
{
  _tx_queue.reset(outgoing_seqnbr);
}

void tcp_connection::reset_rx(tcp_seqnbr incoming_seqnbr)
{
  _rx_queue.reset(incoming_seqnbr);
}

void tcp_connection::send(const tcp_send_segment &segment, const char *data, size_t length)
{
  // TODO: break up the data into multiple IP datagrams according to MTU
  uint16_t flags = segment.flags;
  flags |= ACK;  // TODO: when shouldn't we add an ack?

  uint8_t hdrsz = 5;
  uint16_t flags_hdrsrz = (flags & 0x1FF) | (hdrsz << 12);

  tcp_seqnbr this_seqnbr = segment.seqnbr;
  tcp_seqnbr remotes_last_seqnbr = _rx_queue.read_cursor();

  tcp_header hdr;
  hdr.src_port = htons(_local.port);
  hdr.dest_port = htons(_remote.port);
  hdr.seq_nbr = htonl(this_seqnbr);
  hdr.ack_nbr = htonl(remotes_last_seqnbr);
  hdr.flags_hdrsz = htons(flags_hdrsrz);
  hdr.wndsz = htons(10000); // TODO
  hdr.checksum = 0;
  hdr.urgptr = 0;

  ipv4_dgram ipv4;
  ipv4.ttl = 65;
  ipv4.src_addr = _local.ipaddr;
  ipv4.dest_addr = _remote.ipaddr;
  ipv4.proto = PROTO_TCP;

  if ((((segment.flags & SYN) || (segment.flags & FIN)) && length == 1) || length == 0) {
    // Phantom byte (during handshake) or possibly an idle ack with
    // empty payload
    log(tcp_connection, "send: sending empty or phantom message, seqnbr=% 12d, ack=% 12d",
        this_seqnbr,
        remotes_last_seqnbr);

    hdr.checksum = tcp_checksum(ipv4.src_addr,
                                ipv4.dest_addr,
                                ipv4.proto,
                                (char *)&hdr,
                                sizeof(hdr),
                                nullptr,
                                0);

    ipv4_send(connection_table().interface(), ipv4, (char *)&hdr, sizeof(hdr));
  }
  else {
    hdr.checksum = ipv4_checksum((char *)&hdr, sizeof(hdr), data, length);

    // TODO: there's a lot of copies all the way down to ethernet, improve that
    char buffer[1500];
    assert(length + sizeof(hdr) < sizeof(buffer));
    memcpy(buffer, &hdr, sizeof(hdr));
    memcpy(buffer + sizeof(hdr), data, length);

    ipv4_send(connection_table().interface(), ipv4, buffer, length + sizeof(hdr));
  }

  _next_outgoing_seqnbr = this_seqnbr + length;
}

void tcp_connection::transition(const tcp_connection_state *new_state)
{
  log(tcp, "transitioning from %s to %s", _state->name(), new_state->name());
  _state = new_state;
}

void tcp_connection::tick(int dt)
{
  (void)dt;
}

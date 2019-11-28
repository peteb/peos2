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

  char buffer[10 * 1024]; // Largest segment is 10K
  // TODO: review this buffer size

  while (_rx_queue.has_readable()) {
    tcp_recv_segment sequenced_segment;

    if (!_rx_queue.read_one_segment(&sequenced_segment, buffer, sizeof(buffer))) {
      // This is weird, has_readable returned true...!
      log(tcp, "failed to read one segment");
      return;
    }

    _state->recv(*this, sequenced_segment);
  }
}

void tcp_connection::rx_enqueue(const tcp_segment &segment)
{
  tcp_recv_segment segment_;
  segment_.flags = segment.tcphdr->flags_hdrsz & 0x1FF;
  segment_.seqnbr = segment.tcphdr->seq_nbr;
  _rx_queue.insert(segment_, segment.payload, segment.payload_size);
  // TODO: handle result? What if the queue is full?
}

void tcp_connection::tx_enqueue(const tcp_send_segment &segment, const char *data, size_t length)
{
  _tx_queue.write_back(segment, data, length);

  while (_tx_queue.has_readable()) {
    char buf[10 * 1024];
    tcp_send_segment seg;

    if (size_t bytes_read = _tx_queue.read_one_segment(&seg, buf, sizeof(buf)); bytes_read > 0) {
      send(seg, buf, bytes_read);
    }
  }
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

  tcp_header hdr;
  hdr.src_port = htons(_local.port);
  hdr.dest_port = htons(_remote.port);
  hdr.seq_nbr = htonl(segment.seqnbr);
  hdr.ack_nbr = htonl(_rx_queue.read_cursor());
  hdr.flags_hdrsz = htons(flags_hdrsrz);
  hdr.wndsz = htons(10000); // TODO
  hdr.checksum = 0;
  hdr.urgptr = 0;

  ipv4_dgram ipv4;
  ipv4.ttl = 65;
  ipv4.src_addr = _local.ipaddr;
  ipv4.dest_addr = _remote.ipaddr;
  ipv4.proto = PROTO_TCP;

  if ((segment.flags & SYN) && length == 1) {
    // Phantom byte, ignore it. TODO: flag for this?
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
}

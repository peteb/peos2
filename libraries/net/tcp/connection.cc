#include <support/assert.h>
#include <support/logging.h>

#include "tcp/connection_table.h"
#include "tcp/connection.h"
#include "tcp/connection_state.h"
#include "tcp/utils.h"
#include "tcp/protocol.h"

#include "ipv4/protocol.h"

#include "utils.h"
#include "../utils.h"

namespace {
  int count_specific_matches(const net::tcp::endpoint &filter, const net::tcp::endpoint &endpoint)
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

namespace net::tcp {
  int connection::compare(const endpoint &remote, const endpoint &local)
  {
    int remote_match = count_specific_matches(_remote, remote);
    int local_match = count_specific_matches(_local, local);

    if (remote_match >= 0 && local_match >= 0)
      return remote_match + local_match;
    else
      return -1;
  }

  void connection::on_receive(const segment_metadata &metadata, const char *data, size_t length)
  {
    assert(_state);
    _state->early_recv(*this, metadata, data, length);

    // Automatically use the ack seqnbr from remote to stop our
    // retransmission of packets
    if (metadata.flags & ACK) {
      _tx_queue.ack(metadata.tcp_header->ack_nbr);

      if (metadata.tcp_header->ack_nbr >= _tx_queue.write_cursor()) {
        // Remote has ACK'd everything we've sent
        _state->remote_consumed_all(*this);
      }
    }

    // The sender's segment will be ack'd by the tx queue automatically
    // on transmission or after a timeout
    step();
  }

  void connection::on_full_receive(const char *data, size_t length)
  {
    if (_callback)
      _callback->on_receive(*this, data, length);
  }

  void connection::mark_for_destruction()
  {
    _connection_table.finish_connection(handle);
  }

  void connection::step()
  {
    static char buffer[20 * 1024];
    // TODO: review this buffer size
    // TODO: change static when this is multithreaded

    const bool received_sequenced = _rx_queue.has_readable();

    while (_rx_queue.has_readable()) {
      tcp_recv_segment sequenced_segment;

      if (!_rx_queue.read_one_segment(&sequenced_segment, buffer, sizeof(buffer))) {
        // This is weird, has_readable returned true...!
        log_warn("failed to read one segment");
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

      log_debug("sending empty ack with seqnbr=% 12d", _next_outgoing_seqnbr);
      tcp_send_segment segment;
      segment.flags = 0;
      segment.seqnbr = _next_outgoing_seqnbr;
      send(segment.flags, segment.seqnbr, nullptr, 0);
    }

    // Send outgoing messages if we have any
    while (_tx_queue.has_readable()) {
      log_debug("sending outstanding tx");
      tcp_send_segment seg;
      size_t bytes_read = _tx_queue.read_one_segment(&seg, buffer, sizeof(buffer));
      send(seg.flags, seg.seqnbr, buffer, bytes_read);
    }
  }

  void connection::sequence(const segment_metadata &metadata, const char *data, size_t length)
  {
    tcp_recv_segment segment_;
    segment_.flags = metadata.flags;
    segment_.seqnbr = metadata.tcp_header->seq_nbr;

    _rx_queue.insert(segment_, data, length);
    // TODO: handle result? What if the queue is full?
  }

  void connection::transmit(const tcp_send_segment &segment, const char *data, size_t length, size_t send_length)
  {
    tcp_send_segment segment_(segment);
    segment_.seqnbr = _tx_queue.write_cursor();
    _tx_queue.write_back(segment_, data, length, send_length);
  }

  void connection::reset_tx(net::tcp::sequence_number outgoing_seqnbr)
  {
    _tx_queue.reset(outgoing_seqnbr);
  }

  void connection::reset_rx(net::tcp::sequence_number incoming_seqnbr)
  {
    _rx_queue.reset(incoming_seqnbr);
  }

  void connection::send(uint16_t flags, net::tcp::sequence_number seqnbr, const char *data, size_t length)
  {
    // TODO: break up the data into multiple IP datagrams according to MTU
    flags |= ACK;  // TODO: when shouldn't we add an ack?

    uint8_t hdrsz = 5;
    uint16_t flags_hdrsrz = (flags & 0x1FF) | (hdrsz << 12);

    sequence_number this_seqnbr = seqnbr;
    sequence_number remotes_last_seqnbr = _rx_queue.read_cursor();

    log_debug("send: sending segment with flags %04x:", flags);

    header hdr;
    hdr.src_port = htons(_local.port);
    hdr.dest_port = htons(_remote.port);
    hdr.seq_nbr = htonl(this_seqnbr);
    hdr.ack_nbr = htonl(remotes_last_seqnbr);
    hdr.flags_hdrsz = htons(flags_hdrsrz);
    hdr.wndsz = htons(10000); // TODO
    hdr.checksum = 0;
    hdr.urgptr = 0;

    if ((((flags & SYN) || (flags & FIN)) && length == 1) || length == 0) {
      // Phantom byte (during handshake) or possibly an idle ack with
      // empty payload
      log_debug("send: sending empty or phantom message, seqnbr=% 12d, ack=% 12d",
          this_seqnbr,
          remotes_last_seqnbr);

      hdr.checksum = net::tcp::checksum(_ipv4.local_address(),
                                        _remote.ipaddr,
                                        net::ipv4::proto::PROTO_TCP,
                                        (char *)&hdr,
                                        sizeof(hdr),
                                        nullptr,
                                        0);
      _ipv4.send(net::ipv4::proto::PROTO_TCP, _remote.ipaddr, reinterpret_cast<const char *>(&hdr), sizeof(hdr));
    }
    else {
      log_debug("send: sending segment with payload, length=%d", length);

      hdr.checksum = net::tcp::checksum(_ipv4.local_address(),
                                        _remote.ipaddr,
                                        net::ipv4::proto::PROTO_TCP,
                                        (char *)&hdr,
                                        sizeof(hdr),
                                        data,
                                        length);

      // TODO: there's a lot of copies all the way down to ethernet, improve that
      char buffer[1500];
      assert(length + sizeof(hdr) < sizeof(buffer));
      memcpy(buffer, &hdr, sizeof(hdr));
      memcpy(buffer + sizeof(hdr), data, length);
      _ipv4.send(net::ipv4::proto::PROTO_TCP, _remote.ipaddr, buffer, length + sizeof(hdr));
    }

    _next_outgoing_seqnbr = this_seqnbr + length;
  }

  void connection::transition(const connection_state *new_state)
  {
    log_debug("transitioning from %s to %s", _state->name(), new_state->name());
    _state = new_state;
  }

  void connection::tick(int dt)
  {
    (void)dt;
  }

  void connection::close()
  {
    _state->active_close(*this);
  }

  void connection::set_callback(net::tcp::callback *callback_)
  {
    _callback = callback_;
  }

  void connection::send(const char *data, size_t length)
  {
    tcp_send_segment info{};
    transmit(info, data, length, length);
  }
}

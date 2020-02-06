#include <support/logging.h>

#include "net/protocol_stack.h"
#include "tcp/protocol.h"
#include "tcp/connection_state.h"
#include "../utils.h"

namespace net::tcp {
  protocol::protocol(net::protocol_stack &protocols)
  : _protocols(protocols),
    _connections(protocols.ipv4())
  {
  }

  void protocol::on_receive(const net::ipv4::datagram_metadata &ipv4_metadata, const char *data, size_t length)
  {
    if (length < sizeof(header)) {
      log_debug("dropping datagram due to length less than header");
      return;
    }

    header hdr;
    memcpy(&hdr, data, sizeof(hdr));
    hdr.dest_port = ntohs(hdr.dest_port);
    hdr.src_port = ntohs(hdr.src_port);
    hdr.seq_nbr = ntohl(hdr.seq_nbr);
    hdr.ack_nbr = ntohl(hdr.ack_nbr);
    hdr.wndsz = ntohs(hdr.wndsz);
    hdr.urgptr = ntohs(hdr.urgptr);
    hdr.checksum = ntohs(hdr.checksum);
    hdr.flags_hdrsz = ntohs(hdr.flags_hdrsz);

    uint16_t flags = hdr.flags_hdrsz & 0x1FF;
    uint8_t hdrsz = (hdr.flags_hdrsz >> 12) * 4;

    log_debug("dest_port=%d,src_port=%d,seq_nbr=%d,ack_nbr=%d,wndsz=%d,flags=%x,hdrsz=%d,length=%d",
        hdr.dest_port, hdr.src_port, hdr.seq_nbr, hdr.ack_nbr, hdr.wndsz, flags, hdrsz, length);

    endpoint remote{ipv4_metadata.src_addr, hdr.src_port};
    endpoint local{ipv4_metadata.dest_addr, hdr.dest_port};

    if (auto conn = _connections.find_best_match(remote, local); conn != _connections.end()) {
      size_t total_header_size = ALIGN_UP(hdrsz, 4);

      segment_metadata tcp_metadata;
      tcp_metadata.datagram = &ipv4_metadata;
      tcp_metadata.tcp_header = &hdr;
      tcp_metadata.flags = flags;

      _connections[conn].on_receive(tcp_metadata, data + total_header_size, length - total_header_size);
      _connections.step_new_connections();
      _connections.destroy_finished_connections();
    }
    else {
      log_debug("no connections matched");
    }
  }

  void protocol::listen(const net::tcp::endpoint &local)
  {
    _connections.create_connection(endpoint::wildcard, local, connection_state::LISTEN);
  }

  void protocol::set_callback(callback *callback_)
  {
    _connections.set_callback(callback_);
  }
}

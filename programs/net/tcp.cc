#include <stdint.h>

#include "tcp.h"
#include "utils.h"
#include "tcp_connection_table.h"

class ipv4_impl : public ipv4_interface {
public:
  size_t send(int interface,
              const ipv4_dgram &ipv4,
              const char *data,
              size_t length)
  {
    return ipv4_send(interface, ipv4, data, length);
  }
};

// State
static tcp_connection_table connections;
static ipv4_impl ipv4;

void tcp_init(int interface)
{
  connections.set_ipv4(&ipv4);

  // TODO: fix multi-interface handling
  connections.set_interface(interface);
  connections.create_connection({0, 0}, {0, 5555}, tcp_connection_state::LISTEN);
}

void tcp_recv(int interface, eth_frame *frame, ipv4_dgram *datagram, const char *data, size_t length)
{
  (void)interface;
  // TODO: multi interface support

  if (length < sizeof(tcp_header)) {
    log(tcp, "length less than header, dropping");
    return;
  }

  tcp_header hdr;
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

  log(tcp, "dest_port=%d,src_port=%d,seq_nbr=%d,ack_nbr=%d,wndsz=%d,flags=%x,hdrsz=%d,length=%d",
      hdr.dest_port, hdr.src_port, hdr.seq_nbr, hdr.ack_nbr, hdr.wndsz, flags, hdrsz, length);

  tcp_endpoint remote{datagram->src_addr, hdr.src_port};
  tcp_endpoint local{datagram->dest_addr, hdr.dest_port};

  if (auto conn = connections.find_best_match(remote, local); conn != connections.end()) {
    size_t total_header_size = ALIGN_UP(hdrsz, 4);
    tcp_segment segment;
    segment.frame = frame;
    segment.datagram = datagram;
    segment.tcphdr = &hdr;
    segment.payload = data + total_header_size;
    segment.payload_size = length - total_header_size;
    segment.flags = flags;

    connections[conn].recv(segment);
    connections.step_new_connections();
    connections.destroy_finished_connections();
  }
  else {
    log(tcp, "no connections matched");
  }
}

void tcp_tick(int dt)
{
  connections.tick(dt);
}

#include <stdint.h>

#include "tcp.h"
#include "utils.h"

#define FIN 0x0001
#define SYN 0x0002
#define RST 0x0004
#define PSH 0x0008
#define ACK 0x0010
#define URG 0x0020
#define ECE 0x0040
#define CWR 0x0080
#define NS  0x0100

struct header {
  uint16_t src_port;
  uint16_t dest_port;
  uint32_t seq_nbr;
  uint32_t ack_nbr;
  uint16_t flags_hdrsz;
  uint16_t wndsz;
  uint16_t checksum;
  uint16_t urgptr;
} __attribute__((packed));

void tcp_recv(int interface, eth_frame *frame, ipv4_dgram *datagram, const char *data, size_t length)
{
  (void)interface;
  (void)frame;
  (void)datagram;
  (void)data;
  (void)length;

  log(tcp, "rx datagram");

  if (length < sizeof(header)) {
    log(tcp, "length less than header, dropping");
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
  uint8_t hdrsz = hdr.flags_hdrsz >> 12;

  log(tcp, "dest_port=%d,src_port=%d,seq_nbr=%d,ack_nbr=%d,wndsz=%d,flags=%x,hdrsz=%x",
      hdr.dest_port, hdr.src_port, hdr.seq_nbr, hdr.ack_nbr, hdr.wndsz, flags, hdrsz);
}

#include <stdint.h>
#include <support/utils.h>

#include "udp.h"
#include "utils.h"

struct header {
  uint16_t src_port, dest_port;
  uint16_t length, checksum;
} __attribute__((packed));

void udp_recv(int interface, eth_frame *frame, ipv4_info *datagram, const char *data, size_t length)
{
  (void)interface;
  (void)frame;
  (void)datagram;

  assert(length >= sizeof(header));
  header hdr;
  memcpy(&hdr, data, sizeof(hdr));
  hdr.src_port = ntohs(hdr.src_port);
  hdr.dest_port = ntohs(hdr.dest_port);
  hdr.length = ntohs(hdr.length);
  hdr.checksum = ntohs(hdr.checksum);

  log(udp, "udp src_port=%d,dest_port=%d,length=%d,checksum=%x", hdr.src_port, hdr.dest_port, hdr.length, hdr.checksum);

  const char *payload = data + sizeof(hdr);
  size_t payload_size = hdr.length - sizeof(hdr);

  (void)payload;
  (void)payload_size;
}

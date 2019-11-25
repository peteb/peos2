#include "tcp.h"
#include "utils.h"

void tcp_recv(int interface, eth_frame *frame, ipv4_dgram *datagram, const char *data, size_t length)
{
  (void)interface;
  (void)frame;
  (void)datagram;
  (void)data;
  (void)length;

  log(tcp, "rx datagram");
}

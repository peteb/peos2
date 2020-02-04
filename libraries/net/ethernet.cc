#include <support/format.h>
#include <support/logging.h>

#include <stdint.h>

#include "utils.h"
#include "ethernet.h"
#include "arp.h"
#include "ipv4.h"
#include "tcp.h"

struct header {
  uint8_t  mac_dest[6];
  uint8_t  mac_src[6];
  uint16_t ether_type;
} __attribute__((packed));

static uint8_t local_hwaddr[6];

using namespace p2;

void eth_configure(int interface, uint8_t *local_hwaddr)
{
  (void)interface;
  // TODO: care about the interface
  log_info("config local_hwaddr=%s", hwaddr_str(local_hwaddr).c_str());
  memcpy(local_hwaddr, local_hwaddr, 6);
}

void eth_on_receive(int interface, const char *data, size_t size)
{
  (void)interface;
  // TODO: care about the interface

  header hdr;
  uint16_t pdu_size = size - sizeof(hdr);
  const char *pdu = data + sizeof(hdr);

  memcpy(&hdr, data, sizeof(hdr));
  uint16_t ether_type = ntohs(hdr.ether_type);

  log(eth, "rx pdusz=%d,typ=%04x,dst=%s,src=%s",
      pdu_size, ether_type, hwaddr_str(hdr.mac_dest).c_str(), hwaddr_str(hdr.mac_src).c_str());

  eth_frame frame = {
    .dest = hdr.mac_dest,
    .src = hdr.mac_src,
    .type = ether_type
  };

  switch (ether_type) {
  case _ET_IPV4:
    // TODO: handle ipv4
    ipv4_on_receive(interface, &frame, pdu, pdu_size);
    break;

  case _ET_IPV6:
    // TODO: handle ipv6
    break;

  case _ET_ARP:
    arp_recv(interface, &frame, pdu, pdu_size);
    break;

  case _ET_FLOW:
    // TODO: handle ethernet flow
    break;
  }
}

void eth_hwaddr(int fd, uint8_t *octets)
{
  (void)fd;
  // TODO: care about the interface
  memcpy(octets, local_hwaddr, 6);
}

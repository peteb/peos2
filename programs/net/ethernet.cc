#include <support/userspace.h>
#include <support/format.h>
#include <kernel/syscall_decls.h>
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
  log(eth, "config local_hwaddr=%s", hwaddr_str(local_hwaddr).c_str());
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
  case ET_IPV4:
    // TODO: handle ipv4
    ipv4_on_receive(interface, &frame, pdu, pdu_size);
    break;

  case ET_IPV6:
    // TODO: handle ipv6
    break;

  case ET_ARP:
    arp_recv(interface, &frame, pdu, pdu_size);
    break;

  case ET_FLOW:
    // TODO: handle ethernet flow
    break;
  }
}

int eth_send(int fd, eth_frame *frame, const char *data, size_t size)
{
  log(eth, "tx pdusz=%d,dst=%s,src=%s",
      size, hwaddr_str(frame->dest).c_str(), hwaddr_str(frame->src).c_str());

  // TODO: looping writes
  header hdr;
  memcpy(hdr.mac_dest, frame->dest, 6);
  memcpy(hdr.mac_src, frame->src, 6);
  hdr.ether_type = htons(frame->type);

  // TODO: return failure rather than verify
  uint16_t packet_size = sizeof(hdr) + size;
  verify(syscall3(write, fd, (char *)&packet_size, sizeof(packet_size)));
  verify(syscall3(write, fd, (char *)&hdr, sizeof(hdr)));
  verify(syscall3(write, fd, data, size));
  return 1;
}

void eth_hwaddr(int fd, uint8_t *octets)
{
  (void)fd;
  // TODO: care about the interface
  memcpy(octets, local_hwaddr, 6);
}

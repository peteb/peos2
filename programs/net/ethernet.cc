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

using namespace p2;

void eth_run(int fd)
{
  uint8_t mac[6];
  verify(syscall4(control, fd, CTRL_NET_HW_ADDR, (uintptr_t)mac, 0));
  log(eth, "hwaddr=%s", hwaddr_str(mac).c_str());

  char pdu[1500];
  uint16_t packet_size = 0;
  const int timeout_duration = 200;

  uint64_t last_tick = 0;
  syscall1(currenttime, &last_tick);

  while (true) {
    verify(syscall1(set_timeout, timeout_duration));
    int ret = read(fd, (char *)&packet_size, 2);

    // TODO: can we skip the syscall here? let the kernel write to the
    // variable immediately somehow?

    uint64_t this_tick = 0;
    syscall1(currenttime, &this_tick);

    int tick_delta_ms = this_tick - last_tick;
    last_tick = this_tick;

    arp_tick(tick_delta_ms);
    ipv4_tick(tick_delta_ms);
    tcp_tick(tick_delta_ms);

    if (ret == ETIMEOUT) {
      continue;
    }
    else if (ret < 0) {
      log(eth, "read failed");
      return;
    }

    header hdr;
    uint16_t pdu_size = packet_size - sizeof(hdr);

    // TODO: read larger chunks and parse out in-memory without doing syscalls all the time
    verify(read(fd, (char *)&hdr, sizeof(hdr)));
    verify(read(fd, pdu, pdu_size));

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
      ipv4_on_receive(fd, &frame, pdu, pdu_size);
      break;

    case ET_IPV6:
      // TODO: handle ipv6
      break;

    case ET_ARP:
      arp_recv(fd, &frame, pdu, pdu_size);
      break;

    case ET_FLOW:
      // TODO: handle ethernet flow
      break;
    }
  }
}

void eth_hwaddr(int fd, uint8_t *octets)
{
  // TODO: cache the address locally, this is very wasteful
  verify(syscall4(control, fd, CTRL_NET_HW_ADDR, (uintptr_t)octets, 0));
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

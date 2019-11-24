#include <support/userspace.h>
#include <support/format.h>
#include <kernel/syscall_decls.h>
#include <stdint.h>

#include "utils.h"
#include "ethernet.h"

struct header {
  uint8_t  mac_dest[6];
  uint8_t  mac_src[6];
  uint16_t ether_type;
} __attribute__((packed));

static p2::string<32> hwaddr_str(uint8_t *components);

using namespace p2;

void eth_run(int fd)
{
  uint8_t mac[6];
  verify(syscall4(control, fd, CTRL_NET_HW_ADDR, (uintptr_t)mac, 0));
  log(eth, "hwaddr=%s", hwaddr_str(mac).c_str());

  char pdu[1500];
  uint16_t packet_size = 0;

  while (verify(read(fd, (char *)&packet_size, 2)) > 0) {
    header hdr;
    uint16_t pdu_size = packet_size - sizeof(hdr);

    // TODO: read larger chunks and parse out in-memory without doing syscalls all the time
    verify(read(fd, (char *)&hdr, sizeof(hdr)));
    verify(read(fd, pdu, pdu_size));

    log(eth, "rx pdusz=%d,typ=%04x,dst=%s,src=%s",
        pdu_size, hdr.ether_type, hwaddr_str(hdr.mac_dest).c_str(), hwaddr_str(hdr.mac_src).c_str());
  }

}


static p2::string<32> hwaddr_str(uint8_t *parts)
{
  return p2::format<32>("%02x:%02x:%02x:%02x:%02x:%02x",
                        (uint32_t)parts[0], (uint32_t)parts[1], (uint32_t)parts[2], (uint32_t)parts[3],
                        (uint32_t)parts[4], (uint32_t)parts[5]).str();
}

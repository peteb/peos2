#include <support/string.h>
#include <support/format.h>
#include <support/userspace.h>
#include <support/keyvalue.h>
#include <kernel/syscall_decls.h>
#include <stdint.h>

#include <net/ethernet.h>
#include <net/utils.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/tcp.h>

using namespace p2;

extern "C" void _init();
static void received_data(tcp_connection_handle connection, const char *data, size_t length);
static void feed_data(int fd);
static void fetch_hwaddr(int fd, uint8_t *octets);

int main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  _init();
  puts("Welcome to the network stack!");

  const char *ipaddr = "10.14.0.3";
  const char *netmask = "255.255.255.255";
  const char *gwaddr = "10.14.0.1";

  if (argc > 1) {
    log(nsa, "parsing attributes '%s'...", argv[1]);
    p2::keyvalue<256> opts(argv[1]);

    if (opts["ipaddr"]) ipaddr = opts["ipaddr"];
    if (opts["netmask"]) netmask = opts["netmask"];
    if (opts["gw"]) gwaddr = opts["gw"];
  }

  int fd = verify(syscall2(open, "/dev/eth0", 0));

  uint8_t hwaddr[6];
  fetch_hwaddr(fd, hwaddr);

  eth_configure(fd, hwaddr);
  ipv4_configure(fd,
                 parse_ipaddr(ipaddr),
                 parse_ipaddr(netmask),
                 parse_ipaddr(gwaddr));
  tcp_init(fd);

  tcp_connection_listeners listeners;
  listeners.on_receive = received_data;
  tcp_listen(8080, listeners);

  feed_data(fd);

  verify(syscall1(close, fd));

  return 0;
}

START(main);

void received_data(tcp_connection_handle connection, const char *data, size_t length)
{
  char buffer[256];
  memcpy(buffer, data, p2::min(length, sizeof(buffer)));
  buffer[p2::min(length, sizeof(buffer) - 1)] = 0;
  log(net, "rx seq segment %d bytes: %s", length, buffer);

  // Send some dummy data
  const char *message = "HTTP/1.1 200 OK\r\n"
    "Server: peos2\r\n"
    "Content-Length: 17\r\n"
    "Content-Type: text/plain\r\n"
    "Connection: Closed\r\n"
    "\r\n"
    "Handled by peos2\n";

  tcp_send(connection, message, strlen(message));
  tcp_close(connection);
}

void feed_data(int fd)
{
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
      log(net, "read failed");
      return;
    }

    char pdu[1518];
    ret = read(fd, pdu, packet_size);

    if (ret < 0) {
      log(net, "read failed");
    }

    eth_on_receive(fd, pdu, packet_size);
  }
}

void fetch_hwaddr(int fd, uint8_t *octets)
{
  // TODO: cache the address locally, this is very wasteful
  verify(syscall4(control, fd, CTRL_NET_HW_ADDR, (uintptr_t)octets, 0));
}

struct header {
  uint8_t  mac_dest[6];
  uint8_t  mac_src[6];
  uint16_t ether_type;
} __attribute__((packed));


// Below is used from within libnet
int eth_send(int fd, eth_frame *frame, const char *data, size_t size)
{
  log(eth, "tx pdusz=%d,dst=%s,src=%s",
      size, hwaddr_str(frame->dest).c_str(), hwaddr_str(frame->src).c_str());

  // TODO: looping writes
  header hdr;
  memcpy(hdr.mac_dest, frame->dest, 6);
  memcpy(hdr.mac_src, frame->src, 6);
  hdr.ether_type = htons(frame->type);

  (void)fd;
  (void)data;
  (void)size;
  // TODO: return failure rather than verify
  uint16_t packet_size = sizeof(hdr) + size;
  verify(syscall3(write, fd, (char *)&packet_size, sizeof(packet_size)));
  verify(syscall3(write, fd, (char *)&hdr, sizeof(hdr)));
  verify(syscall3(write, fd, data, size));
  return 1;
}

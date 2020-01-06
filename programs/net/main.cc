#include <support/string.h>
#include <support/format.h>
#include <support/userspace.h>
#include <support/keyvalue.h>
#include <kernel/syscall_decls.h>

#include "ethernet.h"
#include "utils.h"
#include "arp.h"
#include "ipv4.h"
#include "tcp.h"

using namespace p2;

extern "C" void _init();
static void received_data(tcp_connection_handle connection, const char *data, size_t length);

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

  ipv4_configure(fd,
                 parse_ipaddr(ipaddr),
                 parse_ipaddr(netmask),
                 parse_ipaddr(gwaddr));
  tcp_init(fd);

  tcp_connection_listeners listeners;
  listeners.on_receive = received_data;
  tcp_listen(8080, listeners);

  eth_run(fd);
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
    "Content-Length: 16\r\n"
    "Content-Type: text/plain\r\n"
    "Connection: Closed\r\n"
    "\r\n"
    "Handled by peos2";

  tcp_send(connection, message, strlen(message));
  tcp_close(connection);
}

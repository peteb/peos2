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

  eth_run(fd);
  verify(syscall1(close, fd));

  return 0;
}

START(main);

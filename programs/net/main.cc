#include <support/string.h>
#include <support/format.h>
#include <support/userspace.h>
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

  int fd = verify(syscall2(open, "/dev/eth0", 0));

  ipv4_configure(fd,
                 parse_ipaddr("10.14.0.3"),
                 parse_ipaddr("255.255.255.255"),
                 parse_ipaddr("10.14.0.1"));
  tcp_init(fd);

  eth_run(fd);
  verify(syscall1(close, fd));

  return 0;
}

START(main);

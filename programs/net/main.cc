#include <support/string.h>
#include <support/format.h>
#include <support/userspace.h>
#include <kernel/syscall_decls.h>

#include "ethernet.h"
#include "utils.h"
#include "arp.h"

using namespace p2;

extern "C" void _init();

int main(int /*argc*/, char */*argv*/[])
{
  _init();
  puts("Welcome to the network stack!");

  int fd = verify(syscall2(open, "/dev/eth0", 0));

  arp_lookup_ipv4(fd, parse_ipaddr("10.0.2.55"));
  eth_run(fd);
  verify(syscall1(close, fd));

  return 0;
}

START(main);

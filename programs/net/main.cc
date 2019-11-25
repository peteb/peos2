#include <support/string.h>
#include <support/format.h>
#include <support/userspace.h>
#include <kernel/syscall_decls.h>

#include "ethernet.h"
#include "utils.h"
#include "arp.h"

using namespace p2;

extern "C" void _init();

int main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  _init();
  puts("Welcome to the network stack!");

  int fd = verify(syscall2(open, "/dev/eth0", 0));

  const char *target = "10.0.2.2";

  retryable<probe_result>::await_fun recvd_addr = [=](probe_result res) {
    if (res == PROBE_TIMEOUT) {
      log(eth, "%s: lookup timed out", target);
    }
    else {
      uint8_t hwaddr[6];
      if (arp_cache_lookup_ipv4(fd, parse_ipaddr(target), hwaddr)) {
        log(eth, "%s: lookup returned %s", target, hwaddr_str(hwaddr).c_str());
      }
    }
  };

  arp_request_lookup_ipv4(fd, parse_ipaddr(target), recvd_addr);

  eth_run(fd);
  verify(syscall1(close, fd));

  return 0;
}

START(main);

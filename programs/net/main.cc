#include <support/string.h>
#include <support/format.h>
#include <support/userspace.h>
#include <kernel/syscall_decls.h>

#include "ethernet.h"
#include "utils.h"

using namespace p2;

int main(int /*argc*/, char */*argv*/[])
{
  puts("Welcome to the network stack!");

  int fd = verify(syscall2(open, "/dev/eth0", 0));
  eth_run(fd);
  verify(syscall1(close, fd));

  return 0;
}

START(main);

#include <support/string.h>
#include <support/format.h>
#include <support/userspace.h>
#include <kernel/syscall_decls.h>

using namespace p2;

static int read(int fd, char *buf, size_t bytes);

int main(int /*argc*/, char */*argv*/[])
{
  puts("Welcome to the network stack!");

  int fd = verify(syscall2(open, "/dev/eth0", 0));

  uint8_t mac[6];
  verify(syscall4(control, fd, CTRL_NET_HW_ADDR, (uintptr_t)mac, 0));
  puts(0, format<64>("eth: hwaddr=%d:%d:%d:%d:%d:%d\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]));

  char buffer[1500];
  uint16_t packet_size = 0;

  while (verify(read(fd, (char *)&packet_size, 2)) > 0) {
    verify(read(fd, buffer, packet_size));
    puts(0, format<64>("eth: rx packet size=%d\n", packet_size));
    syscall3(write, fd, "HEJ", 3);
  }

  verify(syscall1(close, fd));

  return 0;
}

START(main);

static int read(int fd, char *buf, size_t length)
{
  size_t total = 0;
  int ret;

  while ((ret = syscall3(read, fd, buf, length)) > 0) {
    length -= ret;
    total += ret;
  }

  if (ret < 0)
    return ret;
  else
    return total;
}

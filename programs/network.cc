#include <support/string.h>
#include <support/format.h>
#include <support/userspace.h>
#include <kernel/syscall_decls.h>

using namespace p2;

int main(int /*argc*/, char */*argv*/[])
{
  puts("Welcome to the network stack!");

  int fd = verify(syscall2(open, "/dev/eth0", 0));

  int bytes_read;
  char buffer[1500];
  uint16_t packet_size = 0;

  while ((bytes_read = verify(syscall3(read, fd, (char *)&packet_size, 2))) > 0) {
    if (verify(syscall3(read, fd, buffer, packet_size)) != packet_size) {
      puts("Failed to read the packet");
      return 0;
    }

    puts(0, format<64>("eth: rx packet size=%d\n", packet_size));

    syscall3(write, fd, "HEJ", 3);
  }

  verify(syscall1(close, fd));

  return 0;
}

START(main);

#include <support/logging.h>
#include <support/userspace.h>
#include <support/limits.h>

#include <kernel/syscall_decls.h>
#include <net/protocol_stack.h>
#include <net/utils.h>

#include <stdint.h>

using namespace p2;

extern "C" void _init();
static int fetch_hwaddr(int fd, net::ethernet::address *hwaddr);
static void feed_data(int fd, net::protocol_stack &protocols);
static void configure_ethernet(int fd, net::ethernet::protocol &ethernet);

class file_device : public net::device {
public:
  file_device(int fd) : _fd(fd) {}

  int send(const char *data, size_t length) final
  {
    assert(length < p2::numeric_limits<uint16_t>::max());

    uint16_t packet_size = length;
    int ret = syscall3(write, _fd, reinterpret_cast<char *>(&packet_size), sizeof(packet_size));

    if (ret <= 0)
      return ret;

    return syscall3(write, _fd, data, length);
  }

private:
  int _fd;
};

int main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;

  _init();
  puts("Welcome to the network stack!");

  int fd = verify(syscall2(open, "/dev/eth0", 0));

  {
    file_device device(fd);
    net::protocol_stack protocols(&device);
    configure_ethernet(fd, protocols.ethernet());
    protocols.ipv4().configure(parse_ipaddr("10.0.2.15"),
      parse_ipaddr("255.255.255.255"),
      parse_ipaddr("10.0.2.2"));

    feed_data(fd, protocols);
  }

  verify(syscall1(close, fd));

  return 0;
}

START(main);

void configure_ethernet(int fd, net::ethernet::protocol &ethernet)
{
  net::ethernet::address hwaddr;

  if (int error = fetch_hwaddr(fd, &hwaddr); error != 0) {
    log_error("Failed to fetch hwaddr, error=%d", error);
    syscall1(exit, 1);
  }

  ethernet.configure(hwaddr);
}

void feed_data(int fd, net::protocol_stack &protocols)
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
    protocols.tick(tick_delta_ms);

    if (ret == ETIMEOUT) {
      continue;
    }
    else if (ret < 0) {
      log_error("read from ethernet file descriptor failed, error=%d", ret);
      return;
    }

    char pdu[1518];
    ret = read(fd, pdu, packet_size);

    if (ret < 0) {
      log_error("read failed with error=%d", ret);
      return;
    }
    else if (ret != packet_size) {
      log_error("read failed to read all bytes, ret=%d", ret);
      continue;
    }

    protocols.ethernet().on_receive(pdu, packet_size);
  }
}

/*void received_data(tcp_connection_handle connection, const char *data, size_t length)
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

*/

int fetch_hwaddr(int fd, net::ethernet::address *octets)
{
  assert(sizeof(*octets) == 6);
  return syscall4(control, fd, CTRL_NET_HW_ADDR, (uintptr_t)octets, 0);
}

// Used in libraries for logging
void _log_print(int level, const char *message)
{
  (void)level;
  puts(message);
}
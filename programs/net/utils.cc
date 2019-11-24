#include "utils.h"

#include <kernel/syscall_decls.h>
#include <stdint.h>


using namespace p2;

char debug_out_buffer[512];

int read(int fd, char *buf, size_t length)
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

p2::string<32> hwaddr_str(const uint8_t *octets)
{
  return p2::format<32>("%02x:%02x:%02x:%02x:%02x:%02x",
                        (uint32_t)octets[0], (uint32_t)octets[1], (uint32_t)octets[2], (uint32_t)octets[3],
                        (uint32_t)octets[4], (uint32_t)octets[5]).str();
}

p2::string<16> ipaddr_str(uint32_t octets)
{
  return p2::format<16>("%d.%d.%d.%d",
                        (octets >> 24) & 0xFF,
                        (octets >> 16) & 0xFF,
                        (octets >> 8) & 0xFF,
                        (octets >> 0) & 0xFF).str();
}

uint32_t parse_ipaddr(const char *str)
{
  // Supports only the simple format 123.255.1.0.128

  const char *pos = str;
  uint32_t ipaddr = 0;
  int current_octet = 0;

  while (true) {
    if (isdigit(*pos)) {
      current_octet = (current_octet * 10 + (*pos++ - '0'));
    }
    else if (*pos == '.' || *pos == '\0') {
      assert(current_octet >= 0 && current_octet <= 255);
      // TODO: signal parse fail rather than panic
      ipaddr = ((ipaddr << 8) | (current_octet & 0xFF));
      current_octet = 0;

      if (*pos == '\0')
        break;
      else
        ++pos;
    }
  }

  return ipaddr;
}

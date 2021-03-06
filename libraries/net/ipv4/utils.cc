#include <support/format.h>
#include "ipv4/utils.h"
#include "../utils.h"

namespace net::ipv4 {
  p2::string<16> ipaddr_str(net::ipv4::address octets)
  {
    return p2::format<16>("%d.%d.%d.%d",
                          (octets >> 24) & 0xFF,
                          (octets >> 16) & 0xFF,
                          (octets >> 8) & 0xFF,
                          (octets >> 0) & 0xFF).str();
  }

  net::ipv4::address parse_ipaddr(const char *str)
  {
    // Supports only the simple format 123.255.1.0.128
    const char *pos = str;
    net::ipv4::address ipaddr = 0;
    int current_octet = 0;

    while (true) {
      if (p2::isdigit(*pos)) {
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

  uint16_t checksum(const net::ipv4::header &header, const char *options, size_t options_length)
  {
    assert(header.checksum == 0);

    auto sum = sum_words(reinterpret_cast<const char *>(&header), sizeof(header));
    sum += sum_words(options, options_length);

    while (sum > 0xFFFF)
      sum = (sum >> 16) + (sum & 0xFFFF);

    return ~(uint16_t)sum;
  }
}
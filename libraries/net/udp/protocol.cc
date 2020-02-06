#include <support/utils.h>
#include <support/logging.h>

#include "udp/protocol.h"
#include "udp/definitions.h"
#include "ipv4/definitions.h"

#include "utils.h"

namespace net::udp {
  void protocol::on_receive(const net::ipv4::datagram_metadata &datagram, const char *data, size_t length)
  {
    assert(length >= sizeof(header));
    header hdr;
    memcpy(&hdr, data, sizeof(hdr));
    hdr.src_port = ntohs(hdr.src_port);
    hdr.dest_port = ntohs(hdr.dest_port);
    hdr.length = ntohs(hdr.length);
    hdr.checksum = ntohs(hdr.checksum);

    log_debug("udp src_port=%d,dest_port=%d,length=%d,checksum=%x", hdr.src_port, hdr.dest_port, hdr.length, hdr.checksum);

    const char *payload = data + sizeof(hdr);
    size_t payload_size = hdr.length - sizeof(hdr);

    (void)payload;
    (void)payload_size;
    (void)datagram;
  }

}

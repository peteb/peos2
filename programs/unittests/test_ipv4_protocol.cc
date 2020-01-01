#include <support/unittest.h>
#include "net/utils.h"
#include "net/ipv4_protocol.h"

namespace {
ipv4_header basic_header() {
  ipv4_header header;
  header.ihl = 5;
  header.version = 4;
  header.ecn_dscp = 0;
  header.total_len = htons(sizeof(header) + 20);
  header.id = htons(1);
  header.frag_ofs = 0;
  header.ttl = 65;
  header.protocol = 6;
  header.checksum = 0;
  header.src_addr = htonl(parse_ipaddr("10.14.0.3"));
  header.dest_addr = htonl(parse_ipaddr("83.248.217.4"));
  return header;
}
}

TESTSUITE(ipv4_protocol) {
  TESTCASE("ipv4_checksum correctly calculates given scenarios") {
    // These samples have been collected using packet capture and verified

    {
      ipv4_header hdr = basic_header();
      ASSERT_EQ(ipv4_checksum(hdr), ntohs(0x42c2));
    }

    {
      ipv4_header hdr = basic_header();
      hdr.id = htons(5);
      hdr.dest_addr = htonl(parse_ipaddr("217.214.149.216"));
      ASSERT_EQ(ipv4_checksum(hdr), ntohs(0x000c));
    }
  }
}

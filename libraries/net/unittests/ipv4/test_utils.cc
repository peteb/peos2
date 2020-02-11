#include <support/unittest.h>
#include "utils.h"
#include "ipv4/utils.h"

namespace {
  net::ipv4::header basic_header() {
    net::ipv4::header header;
    header.ihl = 5;
    header.version = 4;
    header.ecn_dscp = 0;
    header.total_len = htons(sizeof(header) + 20);
    header.id = htons(1);
    header.frag_ofs = 0;
    header.ttl = 65;
    header.protocol = 6;
    header.checksum = 0;
    header.src_addr = htonl(net::ipv4::parse_ipaddr("10.14.0.3"));
    header.dest_addr = htonl(net::ipv4::parse_ipaddr("83.248.217.4"));
    return header;
  }
}

TESTSUITE(ipv4::utils) {
  TESTCASE("ipv4_checksum correctly calculates given scenarios") {
    // These samples have been collected using packet capture and verified

    {
      net::ipv4::header hdr = basic_header();
      ASSERT_EQ(net::ipv4::checksum(hdr), ntohs(0x42c2));
    }

    {
      net::ipv4::header hdr = basic_header();
      hdr.id = htons(5);
      hdr.dest_addr = htonl(net::ipv4::parse_ipaddr("217.214.149.216"));
      ASSERT_EQ(net::ipv4::checksum(hdr), ntohs(0x000c));
    }
  }
}

#include <support/unittest.h>
#include "ipv4/protocol.h"
#include "ipv4/definitions.h"
#include "ipv4/utils.h"
#include "unittests/testing_utils.h"
#include "utils.h"

namespace {
  class mock {
  public:
    mock()
    {
      protocols._udp = &udp_mock;
      protocols._arp = &arp_mock;
    }

    net::protocol_stack_mock protocols;
    net::udp::protocol_mock udp_mock;
    net::arp::protocol_mock arp_mock;
  };

  net::ipv4::header basic_header(size_t payload_size)
  {
    net::ipv4::header hdr;
    hdr.ihl = 5;
    hdr.version = 4;
    hdr.ecn_dscp = 0;
    hdr.total_len = htons(payload_size + sizeof(hdr));
    hdr.id = htons(1);
    hdr.frag_ofs = htons(0);
    hdr.ttl = 64;
    hdr.protocol = net::ipv4::proto::PROTO_UDP;
    hdr.checksum = 0;
    hdr.src_addr = htonl(net::ipv4::parse_ipaddr("1.1.0.4"));
    hdr.dest_addr = htonl(net::ipv4::parse_ipaddr("1.1.0.5"));
    return hdr;
  }
}

TESTSUITE(net::ipv4::protocol) {
  char data[1024];

  TESTCASE("on_receive: non-fragmented packet is forwarded to UDP") {
    // Given
    mock m;
    net::ipv4::protocol_impl ipv4(m.protocols);
    ipv4.configure(net::ipv4::parse_ipaddr("1.1.0.5"),
                  net::ipv4::parse_ipaddr("255.255.255.0"),
                  net::ipv4::parse_ipaddr("1.1.0.1"));

    const char payload[] = "hello";
    net::ipv4::header hdr = basic_header(sizeof(payload));
    hdr.checksum = net::ipv4::checksum(hdr);
    memcpy(data, &hdr, sizeof(hdr));
    memcpy(data + sizeof(hdr), payload, sizeof(payload));

    // When
    ipv4.on_receive({}, data, sizeof(payload) + sizeof(hdr));

    // Then
    ASSERT_EQ(m.udp_mock.on_receive_invocations.size(), 1u);
    auto invocation = m.udp_mock.on_receive_invocations.front();
    ASSERT_EQ(invocation.data.size(), 6u);
  }

  TESTCASE("on_receive: packet with 0 ttl is dropped") {
    // Given
    mock m;
    net::ipv4::protocol_impl ipv4(m.protocols);
    ipv4.configure(net::ipv4::parse_ipaddr("1.1.0.5"),
                  net::ipv4::parse_ipaddr("255.255.255.0"),
                  net::ipv4::parse_ipaddr("1.1.0.1"));

    const char payload[] = "hello";
    net::ipv4::header hdr = basic_header(sizeof(payload));
    hdr.ttl = 0;
    hdr.checksum = net::ipv4::checksum(hdr);

    memcpy(data, &hdr, sizeof(hdr));
    memcpy(data + sizeof(hdr), payload, sizeof(payload));

    // When
    ipv4.on_receive({}, data, sizeof(payload) + sizeof(hdr));

    // Then
    ASSERT_EQ(m.udp_mock.on_receive_invocations.size(), 0u);
  }

  TESTCASE("on_receive: packet with incorrect checksum is dropped") {
    // Given
    mock m;
    net::ipv4::protocol_impl ipv4(m.protocols);
    ipv4.configure(net::ipv4::parse_ipaddr("1.1.0.5"),
                  net::ipv4::parse_ipaddr("255.255.255.0"),
                  net::ipv4::parse_ipaddr("1.1.0.1"));

    const char payload[] = "hello";
    net::ipv4::header hdr = basic_header(sizeof(payload));
    --hdr.checksum;

    memcpy(data, &hdr, sizeof(hdr));
    memcpy(data + sizeof(hdr), payload, sizeof(payload));

    // When
    ipv4.on_receive({}, data, sizeof(payload) + sizeof(hdr));

    // Then
    ASSERT_EQ(m.udp_mock.on_receive_invocations.size(), 0u);
  }

  TESTCASE("on_receive: packet with different dest ip address is dropped") {
    // Given
    mock m;
    net::ipv4::protocol_impl ipv4(m.protocols);
    ipv4.configure(net::ipv4::parse_ipaddr("1.1.0.5"),
                  net::ipv4::parse_ipaddr("255.255.255.0"),
                  net::ipv4::parse_ipaddr("1.1.0.1"));

    const char payload[] = "hello";
    net::ipv4::header hdr = basic_header(sizeof(payload));
    hdr.dest_addr = htonl(net::ipv4::parse_ipaddr("1.1.0.6"));
    hdr.checksum = net::ipv4::checksum(hdr);
    memcpy(data, &hdr, sizeof(hdr));
    memcpy(data + sizeof(hdr), payload, sizeof(payload));

    // When
    ipv4.on_receive({}, data, sizeof(payload) + sizeof(hdr));

    // Then
    ASSERT_EQ(m.udp_mock.on_receive_invocations.size(), 0u);
  }

  TESTCASE("on_receive: packet with broadcast dest ip address on same subnet is received") {
    // Given
    mock m;
    net::ipv4::protocol_impl ipv4(m.protocols);
    ipv4.configure(net::ipv4::parse_ipaddr("1.1.0.5"),
                  net::ipv4::parse_ipaddr("255.255.255.0"),
                  net::ipv4::parse_ipaddr("1.1.0.1"));

    const char payload[] = "hello";
    net::ipv4::header hdr = basic_header(sizeof(payload));
    hdr.dest_addr = htonl(net::ipv4::parse_ipaddr("1.1.0.255"));
    hdr.checksum = net::ipv4::checksum(hdr);
    memcpy(data, &hdr, sizeof(hdr));
    memcpy(data + sizeof(hdr), payload, sizeof(payload));

    // When
    ipv4.on_receive({}, data, sizeof(payload) + sizeof(hdr));

    // Then
    ASSERT_EQ(m.udp_mock.on_receive_invocations.size(), 1u);
  }

  TESTCASE("on_receive: packet with broadcast dest ip address on different subnet is dropped") {
    // Given
    mock m;
    net::ipv4::protocol_impl ipv4(m.protocols);
    ipv4.configure(net::ipv4::parse_ipaddr("1.1.0.5"),
                  net::ipv4::parse_ipaddr("255.255.255.0"),
                  net::ipv4::parse_ipaddr("1.1.0.1"));

    const char payload[] = "hello";
    net::ipv4::header hdr = basic_header(sizeof(payload));
    hdr.dest_addr = htonl(net::ipv4::parse_ipaddr("1.1.1.255"));
    hdr.checksum = net::ipv4::checksum(hdr);
    memcpy(data, &hdr, sizeof(hdr));
    memcpy(data + sizeof(hdr), payload, sizeof(payload));

    // When
    ipv4.on_receive({}, data, sizeof(payload) + sizeof(hdr));

    // Then
    ASSERT_EQ(m.udp_mock.on_receive_invocations.size(), 0u);
  }

  TESTCASE("on_receive: packet with an oversized total_length is dropped") {
    // Given
    mock m;
    net::ipv4::protocol_impl ipv4(m.protocols);
    ipv4.configure(net::ipv4::parse_ipaddr("1.1.0.5"),
                  net::ipv4::parse_ipaddr("255.255.255.0"),
                  net::ipv4::parse_ipaddr("1.1.0.1"));

    const char payload[] = "hello";
    net::ipv4::header hdr = basic_header(sizeof(payload));
    hdr.total_len = 0xFFFF;
    hdr.checksum = net::ipv4::checksum(hdr);
    memcpy(data, &hdr, sizeof(hdr));
    memcpy(data + sizeof(hdr), payload, sizeof(payload));

    // When
    ipv4.on_receive({}, data, sizeof(payload) + sizeof(hdr));

    // Then
    ASSERT_EQ(m.udp_mock.on_receive_invocations.size(), 0u);
  }
}
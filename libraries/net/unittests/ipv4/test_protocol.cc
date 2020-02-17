#include <support/unittest.h>

#include "ipv4/protocol.h"
#include "ipv4/definitions.h"
#include "ipv4/utils.h"

#include "unittests/testing_utils.h"
#include "unittests/ethernet_data.h"

#include "utils.h"

namespace {
  class mock {
  public:
    mock()
    {
      protocols._udp = &udp_mock;
      protocols._arp = &arp_mock;
      protocols._icmp = &icmp_mock;
    }

    net::protocol_stack_mock protocols;
    net::udp::protocol_mock udp_mock;
    net::arp::protocol_mock arp_mock;
    net::icmp::protocol_mock icmp_mock;
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

  uint16_t create_frag_ofs(uint16_t frag_ofs, net::ipv4::flags flags)
  {
    return htons((frag_ofs / 8) | (flags << 12));
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

  TESTCASE("on_receive: options in a packet are not included in forwarded payload") {
    // Given
    mock m;
    const int option_dwords = 4;
    net::ipv4::protocol_impl ipv4(m.protocols);
    ipv4.configure(net::ipv4::parse_ipaddr("1.1.0.5"),
                  net::ipv4::parse_ipaddr("255.255.255.0"),
                  net::ipv4::parse_ipaddr("1.1.0.1"));

    const char payload[] = "hello";
    net::ipv4::header hdr = basic_header(sizeof(payload) + option_dwords * 4);
    hdr.ihl = 5 + option_dwords;
    hdr.checksum = net::ipv4::checksum(hdr, data + sizeof(hdr), option_dwords * 4);
    memcpy(data, &hdr, sizeof(hdr));
    memcpy(data  + sizeof(hdr) + option_dwords * 4, payload, sizeof(payload));

    // When
    ipv4.on_receive({}, data, sizeof(payload) + option_dwords * 4 + sizeof(hdr));

    // Then
    ASSERT_EQ(m.udp_mock.on_receive_invocations.size(), 1u);
    auto invocation = m.udp_mock.on_receive_invocations.front();
    ASSERT_EQ(invocation.data.size(), 6u);
    ASSERT_EQ(memcmp(invocation.data.data(), payload, sizeof(payload)), 0);
  }

  TESTCASE("on_receive: two fragments received in order are reassembled") {
    // Given
    mock m;
    net::ipv4::protocol_impl ipv4(m.protocols);
    ipv4.configure(net::ipv4::parse_ipaddr("1.1.0.5"),
                  net::ipv4::parse_ipaddr("255.255.255.0"),
                  net::ipv4::parse_ipaddr("1.1.0.1"));

    const char payload[] = "abcdefghijklmnopqrstuvwxyz";

    // First fragment
    {
      net::ipv4::header hdr = basic_header(8);
      hdr.frag_ofs = create_frag_ofs(0, net::ipv4::FLAGS_MF);
      hdr.checksum = net::ipv4::checksum(hdr);
      memcpy(data, &hdr, sizeof(hdr));
      memcpy(data + sizeof(hdr), payload, 8);
      ipv4.on_receive({}, data, 8 + sizeof(hdr));
    }

    // When/second fragment
    {
      net::ipv4::header hdr = basic_header(16);
      hdr.frag_ofs = create_frag_ofs(8, net::ipv4::FLAGS_NONE);
      hdr.checksum = net::ipv4::checksum(hdr);
      memcpy(data, &hdr, sizeof(hdr));
      memcpy(data + sizeof(hdr), payload + 8, 16);
      ipv4.on_receive({}, data, 16 + sizeof(hdr));
    }

    // Then
    ASSERT_EQ(m.udp_mock.on_receive_invocations.size(), 1u);
    auto invocation = m.udp_mock.on_receive_invocations.front();
    ASSERT_EQ(invocation.data.size(), 24u);
    ASSERT_EQ(memcmp(invocation.data.data(), payload, 24), 0);
  }

  TESTCASE("on_receive: two fragments received in reverse order are reassembled") {
    // Given
    mock m;
    net::ipv4::protocol_impl ipv4(m.protocols);
    ipv4.configure(net::ipv4::parse_ipaddr("1.1.0.5"),
                  net::ipv4::parse_ipaddr("255.255.255.0"),
                  net::ipv4::parse_ipaddr("1.1.0.1"));

    const char payload[] = "abcdefghijklmnopqrstuvwxyz";

    // When/second fragment
    {
      net::ipv4::header hdr = basic_header(16);
      hdr.frag_ofs = create_frag_ofs(8, net::ipv4::FLAGS_NONE);
      hdr.checksum = net::ipv4::checksum(hdr);
      memcpy(data, &hdr, sizeof(hdr));
      memcpy(data + sizeof(hdr), payload + 8, 16);
      ipv4.on_receive({}, data, 16 + sizeof(hdr));
    }

    // First fragment
    {
      net::ipv4::header hdr = basic_header(8);
      hdr.frag_ofs = create_frag_ofs(0, net::ipv4::FLAGS_MF);
      hdr.checksum = net::ipv4::checksum(hdr);
      memcpy(data, &hdr, sizeof(hdr));
      memcpy(data + sizeof(hdr), payload, 8);
      ipv4.on_receive({}, data, 8 + sizeof(hdr));
    }

    // Then
    ASSERT_EQ(m.udp_mock.on_receive_invocations.size(), 1u);
    auto invocation = m.udp_mock.on_receive_invocations.front();
    ASSERT_EQ(invocation.data.size(), 24u);
    ASSERT_EQ(memcmp(invocation.data.data(), payload, 24), 0);
  }

  TESTCASE("on_receive: datagram is not reassembled if the reassembly times out") {
    // Given
    mock m;
    net::ipv4::protocol_impl ipv4(m.protocols);
    ipv4.configure(net::ipv4::parse_ipaddr("1.1.0.5"),
                  net::ipv4::parse_ipaddr("255.255.255.0"),
                  net::ipv4::parse_ipaddr("1.1.0.1"));

    const char payload[] = "abcdefghijklmnopqrstuvwxyz";

    // First fragment
    {
      net::ipv4::header hdr = basic_header(8);
      hdr.frag_ofs = create_frag_ofs(0, net::ipv4::FLAGS_MF);
      hdr.checksum = net::ipv4::checksum(hdr);
      memcpy(data, &hdr, sizeof(hdr));
      memcpy(data + sizeof(hdr), payload, 8);
      ipv4.on_receive({}, data, 8 + sizeof(hdr));
    }

    ipv4.tick(1'000'000);

    // When/second fragment
    {
      net::ipv4::header hdr = basic_header(16);
      hdr.frag_ofs = create_frag_ofs(8, net::ipv4::FLAGS_NONE);
      hdr.checksum = net::ipv4::checksum(hdr);
      memcpy(data, &hdr, sizeof(hdr));
      memcpy(data + sizeof(hdr), payload + 8, 16);
      ipv4.on_receive({}, data, 16 + sizeof(hdr));
    }

    ipv4.tick(1'000'000);

    // Then
    ASSERT_EQ(m.udp_mock.on_receive_invocations.size(), 0u);
  }

  TESTCASE("on_receive: flooding with incomplete datagrams is handled well") {
    // Given
    mock m;
    net::ipv4::protocol_impl ipv4(m.protocols);
    ipv4.configure(net::ipv4::parse_ipaddr("1.1.0.5"),
                  net::ipv4::parse_ipaddr("255.255.255.0"),
                  net::ipv4::parse_ipaddr("1.1.0.1"));

    const char payload[] = "abcdefghijklmnopqrstuvwxyz";

    // Send in a bunch of fragments from a bunch of different sources
    for (int a = 0; a < 255; ++a) {
      for (int b = 0; b < 255; ++b) {
        {
          net::ipv4::header hdr = basic_header(16);
          hdr.frag_ofs = create_frag_ofs(8, net::ipv4::FLAGS_NONE);
          hdr.src_addr = (hdr.src_addr & 0xFFFF0000) | a | (b << 8);
          hdr.checksum = net::ipv4::checksum(hdr);
          memcpy(data, &hdr, sizeof(hdr));
          memcpy(data + sizeof(hdr), payload + 8, 16);
          ipv4.on_receive({}, data, 16 + sizeof(hdr));
        }
      }
    }

    // Time out reassembly buffers
    ipv4.tick(1'000'000);

    // Verify that no fragments created a datagram
    ASSERT_EQ(m.udp_mock.on_receive_invocations.size(), 0u);

    // Send in first part of a new fragmented message
    {
      net::ipv4::header hdr = basic_header(8);
      hdr.frag_ofs = create_frag_ofs(0, net::ipv4::FLAGS_MF);
      hdr.checksum = net::ipv4::checksum(hdr);
      memcpy(data, &hdr, sizeof(hdr));
      memcpy(data + sizeof(hdr), payload, 8);
      ipv4.on_receive({}, data, 8 + sizeof(hdr));
    }

    // Complete datagram with a second fragment
    {
      net::ipv4::header hdr = basic_header(16);
      hdr.frag_ofs = create_frag_ofs(8, net::ipv4::FLAGS_NONE);
      hdr.checksum = net::ipv4::checksum(hdr);
      memcpy(data, &hdr, sizeof(hdr));
      memcpy(data + sizeof(hdr), payload + 8, 16);
      ipv4.on_receive({}, data, 16 + sizeof(hdr));
    }

    // Then
    ASSERT_EQ(m.udp_mock.on_receive_invocations.size(), 1u);
    auto invocation = m.udp_mock.on_receive_invocations.front();
    ASSERT_EQ(invocation.data.size(), 24u);
    ASSERT_EQ(memcmp(invocation.data.data(), payload, 24), 0);
  }

  TESTCASE("on_receive: real world data, fragmented icmp ping") {
    // Given
    mock m;
    net::ipv4::protocol_impl ipv4(m.protocols);
    ipv4.configure(net::ipv4::parse_ipaddr("2.1.1.1"),
                  net::ipv4::parse_ipaddr("255.255.255.0"),
                  net::ipv4::parse_ipaddr("2.1.1.2"));

    // When
    constexpr size_t header_size = sizeof(net::ethernet::header);
    ipv4.on_receive({}, net::ethernet_ipv4_icmp_ping_frag1 + header_size, sizeof(net::ethernet_ipv4_icmp_ping_frag1) - 1 - header_size);
    ipv4.on_receive({}, net::ethernet_ipv4_icmp_ping_frag2 + header_size, sizeof(net::ethernet_ipv4_icmp_ping_frag2) - 1 - header_size);

    // Then
    ASSERT_EQ(m.icmp_mock.on_receive_invocations.size(), 1u);
  }

  TESTCASE("on_receive: real world data, option") {
    // Given
    mock m;
    net::ipv4::protocol_impl ipv4(m.protocols);
    ipv4.configure(net::ipv4::parse_ipaddr("127.0.0.1"),
                  net::ipv4::parse_ipaddr("255.255.255.0"),
                  net::ipv4::parse_ipaddr("2.1.1.2"));

    // When
    constexpr size_t header_size = sizeof(net::ethernet::header);
    ipv4.on_receive({}, net::ethernet_ipv4_cipso + header_size, sizeof(net::ethernet_ipv4_cipso) - 1 - header_size);

    // Then
    ASSERT_EQ(m.icmp_mock.on_receive_invocations.size(), 1u);
    auto invocation = m.icmp_mock.on_receive_invocations.front();
    ASSERT_EQ(invocation.data.size(), 64u);
    ASSERT_EQ(memcmp(invocation.data.data(), net::ethernet_ipv4_cipso + 74, 64), 0);
  }
}
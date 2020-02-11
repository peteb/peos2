#include <support/unittest.h>
#include "tcp/connection_table.h"
#include "unittests/testing_utils.h"

TESTSUITE(net::tcp::connection_table) {
  TESTCASE("find_best_match: finds a wildcard connection") {
    // given
    net::ipv4::protocol_mock ipv4_mock;
    net::tcp::connection_table tab{ipv4_mock};
    auto conn = tab.create_connection({0, 0}, {0, 0}, nullptr);

    // when
    auto match = tab.find_best_match({0x11223344, 4124}, {0x10101010, 8888});

    // then
    ASSERT_EQ(match, conn);
  }

  TESTCASE("find_best_match: finds the most specific connection") {
    // given
    net::ipv4::protocol_mock ipv4_mock;
    net::tcp::connection_table tab{ipv4_mock};
    tab.create_connection({0, 0}, {0, 0}, nullptr);
    tab.create_connection({0x44334455, 0}, {0, 0}, nullptr);
    tab.create_connection({0x44334455, 2412}, {0, 0}, nullptr);
    tab.create_connection({0x44334455, 2412}, {0x10101010, 0}, nullptr);

    auto conn = tab.create_connection({0x44334455, 2412}, {0x10101010, 8888}, nullptr);

    // when
    auto match = tab.find_best_match({0x44334455, 2412}, {0x10101010, 8888});

    // then
    ASSERT_EQ(match, conn);
  }

  TESTCASE("find_best_match: finds nothing when there is no match") {
    // given
    net::ipv4::protocol_mock ipv4_mock;
    net::tcp::connection_table tab{ipv4_mock};

    tab.create_connection({0, 0}, {0, 8080}, nullptr);

    // when
    auto match = tab.find_best_match({0x11223344, 4124}, {0x10101010, 8888});

    // then
    ASSERT_EQ(match, tab.end());
  }
}

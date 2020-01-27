#include <support/unittest.h>
#include "tcp_connection_table.h"

TESTSUITE(tcp_connection_table) {
  TESTCASE("find_best_match: finds a wildcard connection") {
    // given
    tcp_connection_table tab;
    auto conn = tab.create_connection({0, 0}, {0, 0}, nullptr, {});

    // when
    auto match = tab.find_best_match({0x11223344, 4124}, {0x10101010, 8888});

    // then
    ASSERT_EQ(match, conn);
  }

  TESTCASE("find_best_match: finds the most specific connection") {
    // given
    tcp_connection_table tab;
    tab.create_connection({0, 0}, {0, 0}, nullptr, {});
    tab.create_connection({0x44334455, 0}, {0, 0}, nullptr, {});
    tab.create_connection({0x44334455, 2412}, {0, 0}, nullptr, {});
    tab.create_connection({0x44334455, 2412}, {0x10101010, 0}, nullptr, {});

    auto conn = tab.create_connection({0x44334455, 2412}, {0x10101010, 8888}, nullptr, {});

    // when
    auto match = tab.find_best_match({0x44334455, 2412}, {0x10101010, 8888});

    // then
    ASSERT_EQ(match, conn);
  }

  TESTCASE("find_best_match: finds nothing when there is no match") {
    // given
    tcp_connection_table tab;
    tab.create_connection({0, 0}, {0, 8080}, nullptr, {});

    // when
    auto match = tab.find_best_match({0x11223344, 4124}, {0x10101010, 8888});

    // then
    ASSERT_EQ(match, tab.end());
  }
}

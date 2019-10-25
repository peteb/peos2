#include "support/pool.h"
#include "unittest.h"

BEGIN_SUITE(p2::pool) {
  TESTCASE("created pool has zero size") {
    struct testobj {int hello; };
    p2::pool<testobj, 32> items;

    ASSERT_EQ(items.size(), 0u);
  }

  TESTCASE("items can be pushed up until max_capacity_of_type - 1") {
    p2::pool<int, 255, uint8_t> items;
    for (int i = 0; i < 255; ++i) {
      items.push_back(i);
    }
  }

  TESTCASE("items can be pushed until the end") {
    p2::pool<int, 4> items;

    // when/then (expect no crash ...)
    items.push_back(1);
    items.push_back(2);
    items.push_back(3);
    items.push_back(4);
  }

  TESTCASE("continuous indexes are allocated") {
    p2::pool<int, 4> items;

    ASSERT_EQ(items.push_back(1), 0);
    ASSERT_EQ(items.push_back(2), 1);
    ASSERT_EQ(items.push_back(3), 2);
    ASSERT_EQ(items.push_back(4), 3);
  }

  TESTCASE("add/remove scenario") {
    // given
    p2::pool<int, 13> items;

    // when
    for (int count = 0; count < 1000; ++count) {
      for (int i = 0; i < 13; ++i) {
        int idx = items.push_back(i);
        ASSERT_EQ(items[idx], i);
        ASSERT_EQ(items.size(), size_t(i + 1));
      }

      // From bottom: using freelist
      for (int i = 0; i < 6; ++i) {
        items.erase(i);
      }

      // From top: using watermark
      for (int i = 12; i >= 6; --i) {
        items.erase(i);
      }
    }

    // then
    ASSERT_EQ(items.size(), 0u);
  }

  TESTCASE("pushing further than the capacity causes panic") {
    // given
    p2::pool<int, 3> items;

    // when/then
    items.push_back(1);
    items.push_back(1);
    items.push_back(1);
    ASSERT_PANIC(items.push_back(1));
  };

} END_SUITE;

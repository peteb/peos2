#include "support/pool.h"
#include "unittest.h"

TESTSUITE(p2::pool) {
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

  TESTCASE("ctors for items are not called at initialization") {
    static int ctor_count = 0;
    struct item_t {
      item_t() {++ctor_count; }
    };

    // given
    p2::pool<item_t, 10> items;

    // then
    ASSERT_EQ(ctor_count, 0);
  }

  TESTCASE("push_back: calls copy-ctor once and default ctor once") {
    static int ctor_count = 0, default_ctor_count = 0;
    struct item_t {
      item_t() {++default_ctor_count; }
      item_t(const item_t &) {++ctor_count; }
    };

    // given
    p2::pool<item_t, 10> items;

    // when
    items.push_back(item_t());

    // then
    ASSERT_EQ(ctor_count, 1);
    ASSERT_EQ(default_ctor_count, 1);
  }

  TESTCASE("emplace_back: does not require copy-constructible objects") {
    struct item_t {
      item_t() = delete;
      item_t(int) {}
      item_t(const item_t &) = delete;
      item_t &operator =(const item_t &) = delete;
    };

    // given
    p2::pool<item_t, 10> items;

    // then
    items.emplace_back(123);
  }

  TESTCASE("emplace_back: ctor is used when given arguments") {
    struct item_t {
      item_t(int one, int two) : one(one), two(two) {}
      int one, two;
    };

    // given
    p2::pool<item_t, 10> items;
    uint16_t idx = items.emplace_back(333, 222);
    ASSERT_EQ(items[idx].one, 333);
    ASSERT_EQ(items[idx].two, 222);
  }

  TESTCASE("emplace_back: default ctor is used when given no arguments") {
    static int ctor_count = 0;
    struct item_t {
      item_t() : value(123) {++ctor_count; }
      item_t(int value) : value(value * 10) {}
      item_t &operator =(const item_t &) = delete;
      int value;
    };

    // given
    p2::pool<item_t, 10> items;

    // when
    uint16_t idx = items.emplace_back();

    // then
    ASSERT_EQ(items[idx].value, 123);
    ASSERT_EQ(ctor_count, 1);
  }
}

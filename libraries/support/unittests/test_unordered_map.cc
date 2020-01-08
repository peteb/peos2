#include "support/unittest.h"
#include "support/unordered_map.h"

TESTSUITE(p2::unordered_map) {
  TESTCASE("find: returns end() when map is empty") {
    p2::unordered_map<int, int, 16> map;
    ASSERT_EQ(map.find(1234), map.end());
  }

  TESTCASE("can find inserted value") {
    p2::unordered_map<int, int, 16> map;

    map.insert(444, 987);
    ASSERT_NEQ(map.find(444), map.end());
    ASSERT_EQ(map.find(444)->value, 987);
  }

  TESTCASE("size: overwritten items don't increase size") {
    p2::unordered_map<int, int, 16> map;

    map[3] = 4;
    map[5] = 4;
    map[3] = 4;

    ASSERT_EQ(map.size(), 2u);
  }

  TESTCASE("operator[]: values can be overwritten") {
    p2::unordered_map<int, int, 16> map;

    map[3] = 1;
    map[5] = 2;
    map[3] = 3;

    ASSERT_EQ(map[3], 3);
  }

  TESTCASE("can be iterated over and entries are overwritten") {
    p2::unordered_map<int *, int, 16> map;
    int key_values[3];

    map[&key_values[0]] = 1;
    map[&key_values[1]] = 2;
    map[&key_values[0]] = 3;
    map[&key_values[2]] = 4;

    for (const auto &[key, value] : map) {
      *key = value;
    }

    ASSERT_EQ(key_values[0], 3);
    ASSERT_EQ(key_values[1], 2);
    ASSERT_EQ(key_values[2], 4);
  }
}

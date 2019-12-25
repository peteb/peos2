#include "unittest.h"
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
}

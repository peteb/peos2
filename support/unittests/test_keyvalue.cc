#include "unittest.h"
#include "support/keyvalue.h"

static p2::string<128> str(const char *msg) {
  ASSERT_NEQ(msg, nullptr);
  return p2::string<128>(msg);
}

TESTSUITE(p2::keyvalue) {
  TESTCASE("can be constructed") {
    p2::keyvalue<10> kv("hello");
  }

  TESTCASE("finds attributes and returns correct values") {
    p2::keyvalue<128> kv("name=Peter moo=fie");
    ASSERT_NEQ(kv["name"], nullptr);
    ASSERT_EQ(str(kv["name"]), str("Peter"));
    ASSERT_EQ(str(kv["moo"]), str("fie"));
  }

  TESTCASE("ignores non-kv value") {
    p2::keyvalue<128> kv("kernel name=Peter moo=fie");
    ASSERT_NEQ(kv["name"], nullptr);
    ASSERT_EQ(str(kv["name"]), str("Peter"));
    ASSERT_EQ(str(kv["moo"]), str("fie"));
  }
}

#include "unittest.h"
#include "support/format.h"

BEGIN_SUITE(p2::format) {
  TESTCASE("empty format string gives empty out string") {
    p2::format<64> fmt("");
    ASSERT_EQ(fmt.str().c_str()[0], '\0');
  }

  TESTCASE("can write to external buffer") {
    char buf[64];
    (p2::format(buf, "Hello%d!") % 123).str();
    ASSERT_EQ(buf[0], 'H');
    ASSERT_EQ(buf[4], 'o');
    ASSERT_EQ(buf[5], '1');
    ASSERT_EQ(buf[8], '!');
    ASSERT_EQ(buf[9], '\0');
  }

  TESTCASE("format without flags results in same string") {
    p2::format<32> fmt("hello");
    ASSERT_EQ(fmt.str().c_str()[0], 'h');
    ASSERT_EQ(fmt.str().c_str()[4], 'o');
    ASSERT_EQ(fmt.str().c_str()[5], '\0');
  }

  TESTCASE("combined flags scenario") {
    p2::format<32> fmt("abc %d def %dgh%dij");
    fmt % 10;
    fmt % 488;
    fmt % 9111;
    ASSERT_EQ(fmt.str(), p2::string<32>("abc 10 def 488gh9111ij"));
  }

  TESTCASE("can format numbers up until the end") {
    p2::format<3> fmt("%d");
    fmt % 12;
    ASSERT_EQ(fmt.str(), p2::string<3>("12"));
  }

  TESTCASE("panics when formatting over the end") {
    p2::string<3> dat;
    p2::format fmt(dat, "%d");

    BEGIN_ASSERT_PANIC;
    fmt % 123;
    fmt.str();
    END_ASSERT_PANIC;
  }

} END_SUITE;

#include "unittest.h"
#include "support/string.h"

TESTSUITE(p2::string) {
  TESTCASE("can be created to a good state with internal memory") {
    // given
    p2::string<64> str;

    // when/then
    ASSERT_EQ(str.size(), 0);
    ASSERT_EQ(str.str()[0], '\0');
  }

  TESTCASE("with internal memory can be appended until end") {
    p2::string<7> str;

    str.append('!');
    str.append('!');
    str.append('!');
    str.append('!');
    str.append('!');
    str.append('!');
  }

  TESTCASE("appending over the end causes panic") {
    p2::string<3> str;
    str.append('#');
    str.append('#');
    ASSERT_PANIC(str.append('#'));
  }

  TESTCASE("can be created over an external buffer") {
    char buf[8];
    p2::string str(buf);

    ASSERT_EQ(str.size(), 0);
    ASSERT_EQ(str.str()[0], '\0');
    ASSERT_EQ(str.str(), buf);
  }

  TESTCASE("external buffer is written to") {
    char buf[16];
    p2::string str(buf);

    str.append("Hello");

    ASSERT_EQ(buf[0], 'H');
    ASSERT_EQ(buf[1], 'e');
    ASSERT_EQ(buf[2], 'l');
    ASSERT_EQ(buf[3], 'l');
    ASSERT_EQ(buf[4], 'o');
    ASSERT_EQ(buf[5], '\0');
    ASSERT_EQ(str.size(), 5);
  }

  TESTCASE("copy from char*") {
    const char *msg = "boof";
    const p2::string<16> str(msg);

    ASSERT_NEQ(str.str(), msg);
    ASSERT_EQ(str.size(), 4);
    ASSERT_EQ(str.str()[0], 'b');
    ASSERT_EQ(str.str()[1], 'o');
    ASSERT_EQ(str.str()[2], 'o');
    ASSERT_EQ(str.str()[3], 'f');
    ASSERT_EQ(str.str()[4], '\0');
  }

  TESTCASE("copy from char* to short string") {
    const p2::string<3> str("boof");

    ASSERT_EQ(str.size(), 2);
    ASSERT_EQ(str.str()[0], 'b');
    ASSERT_EQ(str.str()[1], 'o');
    ASSERT_EQ(str.str()[2], '\0');
  }

  TESTCASE("larger partial copy from char*") {
    const p2::string<3> str("moof", 100);

    ASSERT_EQ(str.size(), 2);
    ASSERT_EQ(str.str()[1], 'o');
    ASSERT_EQ(str.str()[2], '\0');
  }

  TESTCASE("smaller partial copy from char*") {
    const p2::string<10> str("moof", 3);

    ASSERT_EQ(str.size(), 3);
    ASSERT_EQ(str.str()[0], 'm');
    ASSERT_EQ(str.str()[1], 'o');
    ASSERT_EQ(str.str()[2], 'o');
    ASSERT_EQ(str.str()[3], '\0');
  }

  TESTCASE("equality") {
    const p2::string<18> hej("hello");
    ASSERT_TRUE(hej == p2::string<18>("hello"));
    ASSERT_FALSE(hej == p2::string<18>("hell"));
    ASSERT_FALSE(hej == p2::string<18>("hellx"));
    ASSERT_TRUE(p2::string<18>() == p2::string<18>());
    ASSERT_TRUE(p2::string<2>("") == p2::string<2>(""));
  }
}

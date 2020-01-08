#include "support/unittest.h"
#include "support/filesystem.h"
#include "support/string.h"

TESTSUITE(filesystem_dirname) {
  TESTCASE("dirname: returns full string for path missing file component") {
    p2::string<32> str("/hello/path/");
    p2::string<32> dir, base;

    p2::dirname(str, &dir, &base);
    ASSERT_EQ(dir, p2::string<32>("/hello/path"));
    ASSERT_EQ(base, p2::string<32>(""));
  }

  TESTCASE("dirname: returns last component when last char is not a slash") {
    p2::string<32> str("/hello/path");
    p2::string<32> dir, base;

    p2::dirname(str, &dir, &base);
    ASSERT_EQ(dir, p2::string<32>("/hello"));
    ASSERT_EQ(base, p2::string<32>("path"));
  }

  TESTCASE("dirname: returns an empty path on root file") {
    p2::string<32> str("/hello");
    p2::string<32> dir, base;

    p2::dirname(str, &dir, &base);
    ASSERT_EQ(dir, p2::string<32>(""));
    ASSERT_EQ(base, p2::string<32>("hello"));
  }
}

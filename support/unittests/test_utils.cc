#include "unittest.h"
#include "support/utils.h"

TESTSUITE(p2::utils) {
  TESTCASE("strnchr does not return one past the last character") {
    ASSERT_EQ(strnchr("\n", '\n', 0), nullptr);
    ASSERT_EQ(strnchr("h\n", '\n', 1), nullptr);
    ASSERT_EQ(strnchr("moofie\n", '\n', 6), nullptr);
  }

  TESTCASE("strnchr finds the last character") {
    ASSERT_NEQ(strnchr("\n", '\n', 1), nullptr);
    ASSERT_NEQ(strnchr("h\n", '\n', 2), nullptr);
    ASSERT_NEQ(strnchr("moofie\n", '\n', 7), nullptr);

    ASSERT_EQ(*strnchr("\n", '\n', 1), '\n');
    ASSERT_EQ(*strnchr("h\n", '\n', 2), '\n');
    ASSERT_EQ(*strnchr("moofie\n", '\n', 7), '\n');
  }
}

#include "unittest.h"
#include "support/function.h"
#include "support/pool.h"

TESTSUITE(p2::inplace_fun) {
  TESTCASE("can be instantiated in a number of ways") {
    p2::inplace_fun<16, int(int, int)> f1 = [](int a, int b) {return a + b; };
    ASSERT_EQ(f1(2, 4), 6);
  }

  TESTCASE("can mutate referenced variables") {
    int data = 0;
    p2::inplace_fun<16, void()> f1 = [&]() {++data; };
    ASSERT_EQ(data, 0);
    f1();
    ASSERT_EQ(data, 1);
  }

  TESTCASE("can be stored in other containers") {
    p2::pool<p2::inplace_fun<16, int(int, int)>, 10> funs;
    funs.emplace_anywhere([](int a, int b) {return a + b; });
    funs.emplace_anywhere([](int a, int b) {return a * b; });

    ASSERT_EQ(funs[0](3, 6), 9);
    ASSERT_EQ(funs[1](3, 6), 18);
  }
}

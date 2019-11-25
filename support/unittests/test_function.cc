#include "unittest.h"
#include "function.h"
#include "pool.h"

TESTSUITE(p2::inline_fun) {
  TESTCASE("can be instantiated in a number of ways") {
    p2::inline_fun<16, int(int, int)> f1 = [](int a, int b) {return a + b; };
    ASSERT_EQ(f1(2, 4), 6);
  }

  TESTCASE("can mutate referenced variables") {
    int data = 0;
    p2::inline_fun<16, void()> f1 = [&]() {++data; };
    ASSERT_EQ(data, 0);
    f1();
    ASSERT_EQ(data, 1);
  }

  TESTCASE("can be stored in other containers") {
    p2::pool<p2::inline_fun<16, int(int, int)>, 10> funs;
    funs.push_back([](int a, int b) {return a + b; });
    funs.push_back([](int a, int b) {return a * b; });

    ASSERT_EQ(funs[0](3, 6), 9);
    ASSERT_EQ(funs[1](3, 6), 18);
  }
}

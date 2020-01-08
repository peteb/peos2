#include "support/unittest.h"
#include "support/queue.h"

TESTSUITE(p2::queue) {
  TESTCASE("push_back fails on queue with zero capacity") {
    p2::queue<int, 0> q;
    ASSERT_EQ(q.push_back(23), false);
  }

  TESTCASE("push_back returns false when reached capacity") {
    p2::queue<int, 4> q;

    ASSERT_EQ(q.size(), 0);
    ASSERT_EQ(q.push_back(1), true);
    ASSERT_EQ(q.size(), 1);
    ASSERT_EQ(q.push_back(1), true);
    ASSERT_EQ(q.size(), 2);
    ASSERT_EQ(q.push_back(1), true);
    ASSERT_EQ(q.size(), 3);
    ASSERT_EQ(q.push_back(1), true);
    ASSERT_EQ(q.size(), 4);
    ASSERT_EQ(q.push_back(1), false);
    ASSERT_EQ(q.size(), 4);
  }

  TESTCASE("push_back can be immediately poped by pop_front") {
    p2::queue<int, 2> q;

    for (int i = 0; i < 10000; ++i) {
      q.push_back(i);
      ASSERT_EQ(q.pop_front(), i);
      ASSERT_EQ(q.size(), 0);
    }
  }

  TESTCASE("multiple push_back can be poped back by pop_front") {
    p2::queue<int, 3> q;

    for (int i = 0; i < 10000; ++i) {
      for (int a = 0; a < 4; ++a) {
        for (int b = 0; b < a; ++b) {
          ASSERT_EQ(q.push_back(i * b), true);
        }

        ASSERT_EQ(q.size(), a);

        for (int b = 0; b < a; ++b) {
          ASSERT_EQ(q.pop_front(), i * b);
        }
      }
    }
  }
}

#include "support/unittest.h"
#include "support/frag_buffer.h"

TESTSUITE(p2::frag_buffer) {
  TESTCASE("empty buffer has a 0 size") {
    p2::frag_buffer<1024> fb;
    ASSERT_EQ(fb.continuous_size(), 0);
  }

  TESTCASE("can fill using sequential non-overlapping writes") {
    // given
    char data[256];
    p2::frag_buffer<1024> fb;

    // when
    fb.insert(0, data, sizeof(data));
    fb.insert(256, data, sizeof(data));
    fb.insert(512, data, sizeof(data));

    // then
    ASSERT_EQ(fb.continuous_size(), 768);
  }

  TESTCASE("can fill using reverse sequential non-overlapping writes") {
    // given
    char data[256];
    p2::frag_buffer<1024> fb;

    // when
    fb.insert(512, data, sizeof(data));
    fb.insert(256, data, sizeof(data));
    fb.insert(0, data, sizeof(data));

    // then
    ASSERT_EQ(fb.continuous_size(), 768);
  }

  TESTCASE("can fill with various non-overlapping writes") {
    // given
    char data[256];
    p2::frag_buffer<1024> fb;

    // when
    fb.insert(10, data, 1);
    fb.insert(7, data, 3);
    fb.insert(0, data, 1);
    fb.insert(1, data, 6);
    fb.insert(11, data, 120);
    fb.insert(131, data, 4);

    // then
    ASSERT_EQ(fb.continuous_size(), 135);
  }

  TESTCASE("can fill with one byte writes") {
    // given
    char data[256];
    p2::frag_buffer<1024> fb;

    // when
    for (int i = 0; i < 1023; ++i)
      fb.insert(i, data, 1);

    // then
    ASSERT_EQ(fb.continuous_size(), 1023);
  }

  TESTCASE("can fill with overlapping writes") {
    // given
    char data[256];
    p2::frag_buffer<1024> fb;

    // when
    fb.insert(5, data, 100);
    fb.insert(1, data, 40);
    fb.insert(0, data, 2);
    fb.insert(80, data, 150);
    fb.insert(80, data, 150);
    ASSERT_EQ(fb.continuous_size(), 230);

    fb.insert(0, data, 500);
    ASSERT_EQ(fb.continuous_size(), 500);
  }

  TESTCASE("data is copied correctly") {
    // given
    p2::frag_buffer<1024> fb;

    // when
    fb.insert(5, "HEY", 3);
    fb.insert(1, "MOOF", 4);
    fb.insert(0, "!", 1);
    fb.insert(7, "LLO", 3);
    fb.insert(10, "!#!", 3);

    char buf[] = "!MOOFHELLO!#!";
    ASSERT_EQ(memcmp(fb.data(), buf, sizeof(buf) - 1), 0);
  }
}

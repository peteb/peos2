#include <support/unittest.h>
#include "tcp_send_queue.h"

TESTSUITE(tcp_send_queue) {
  TESTCASE("empty queue is not readable") {
    // given
    tcp_send_queue q;

    // when/then
    ASSERT_EQ(q.has_readable(), false);
  }

  TESTCASE("write_back can write more than the send window") {
    // given
    char buf[1000];
    tcp_send_queue q;
    q.set_window(100);

    // when
    ASSERT_EQ(q.write_back({0, 0, 1000, 1000}, buf, 1000, 1000), true);
  }

  TESTCASE("write_back cannot write a gap") {
    // given
    char buf[1000];

    tcp_send_queue q;
    q.write_back({0, 0, 1000, 1000}, buf, 1000, 1000);

    // when
    ASSERT_EQ(q.write_back({0, 1002, 1000, 1000}, buf, 1000, 1000), false);
  }

  TESTCASE("write_back can write back-to-back") {
    // given
    char buf[1000];

    tcp_send_queue q;
    q.write_back({0, 0, 1000, 1000}, buf, 1000, 1000);

    // when/then
    ASSERT_EQ(q.write_back({0, 1000, 1000, 1000}, buf, 1000, 1000), true);
    ASSERT_EQ(q.has_readable(), true);
  }

  TESTCASE("write_back can be read") {
    // given
    tcp_send_queue q;
    ASSERT_EQ(q.write_back({1, 0, 0, 0}, "AAA", 3, 3), true);
    ASSERT_EQ(q.write_back({2, 3, 0, 0}, "BBBBB", 5, 5), true);

    // when
    tcp_send_segment seg;
    char buf[100];
    ASSERT_EQ(q.read_one_segment(&seg, buf, sizeof(buf)), 3u);
    ASSERT_EQ(memcmp(buf, "AAA", 3), 0);
    ASSERT_EQ(seg.flags, 1);
    ASSERT_EQ(q.read_one_segment(&seg, buf, sizeof(buf)), 5u);
    ASSERT_EQ(memcmp(buf, "BBBBB", 5), 0);
    ASSERT_EQ(seg.flags, 2);
  }

  TESTCASE("ack scenario") {
    tcp_send_queue q;
    tcp_send_segment s;
    char buf[100];

    q.reset(0);

    // First add a couple of segments
    ASSERT_EQ(q.write_back({1, 0, 0, 0}, " ", 1, 1), true);
    ASSERT_EQ(q.write_back({2, 1, 0, 0}, "ASD", 3, 3), true);
    ASSERT_EQ(q.write_back({1, 4, 0, 0}, "AS", 2, 2), true);
    ASSERT_EQ(q.write_back({1, 6, 0, 0}, "HEY", 3, 3), true);

    // Ack the first segment
    q.ack(1);

    // Verify next segment
    ASSERT_EQ(q.has_readable(), true);
    ASSERT_EQ(q.read_one_segment(&s, buf, sizeof(buf)), 3u);
    ASSERT_EQ(memcmp(buf, "ASD", 3), 0);
    ASSERT_EQ(s.flags, 2u);

    // Read one more segment
    ASSERT_EQ(q.has_readable(), true);
    ASSERT_EQ(q.read_one_segment(&s, buf, sizeof(buf)), 2u);
    ASSERT_EQ(memcmp(buf, "AS", 2), 0);
    ASSERT_EQ(s.flags, 1u);

    // Ack those two segments
    q.ack(6);

    // Verify last segment
    ASSERT_EQ(q.has_readable(), true);
    ASSERT_EQ(q.read_one_segment(&s, buf, sizeof(buf)), 3u);
    ASSERT_EQ(memcmp(buf, "HEY", 3), 0);
    ASSERT_EQ(s.flags, 1u);

    // Ack the last message
    q.ack(9);

    // Verify no more segments
    ASSERT_EQ(q.has_readable(), false);
  }
}

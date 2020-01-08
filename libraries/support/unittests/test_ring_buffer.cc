#include "support/unittest.h"
#include "support/ring_buffer.h"
#include "support/utils.h"

TESTSUITE(p2::ring_buffer) {
  TESTCASE("queue is initially empty") {
    p2::ring_buffer<256> q;
    ASSERT_EQ(q.size(), 0u);
  }

  TESTCASE("write increases size") {
    char data[256];
    p2::ring_buffer<256> q;

    q.write(data, 10);
    ASSERT_EQ(q.size(), 10u);
    q.write(data, 10);
    ASSERT_EQ(q.size(), 20u);
  }

  TESTCASE("write can use the whole capacity in one write") {
    char data[256];
    p2::ring_buffer<256> q;

    q.write(data, 256);
    ASSERT_EQ(q.size(), 256u);
  }

  TESTCASE("write can use the whole capacity in multiple writes") {
    char data[256];
    p2::ring_buffer<256> q;

    q.write(data, 100);
    q.write(data, 100);
    q.write(data, 10);
    q.write(data, 40);
    q.write(data, 6);
    ASSERT_EQ(q.size(), 256u);
  }

  TESTCASE("read_front can read data written by write when circling around") {
    char in_data[3];
    char out_data[3];
    p2::ring_buffer<256> q;

    for (int i = 0; i < 500; ++i) {
      in_data[0] = (char)(i & 0xFF);
      in_data[1] = in_data[0] * 3;
      in_data[2] = in_data[0] ^ in_data[1];

      q.write(in_data, 3);

      size_t bytes_read = q.read_front(out_data, 3);
      ASSERT_EQ(bytes_read, 3u);
      ASSERT_EQ(memcmp(out_data, in_data, 3), 0);
    }
  }

  TESTCASE("read_front can read data written by write in larger blocks") {
    char in_data[8];
    p2::ring_buffer<256> q;

    for (int i = 0; i < 64; ++i) {
      in_data[0] = (char)(i & 0xFF);
      in_data[1] = in_data[0] * 3;
      in_data[2] = in_data[0] ^ in_data[1];
      in_data[3] = in_data[0] ^ in_data[2];

      bool wrote = q.write(in_data, 4);
      ASSERT_EQ(wrote, true);
    }

    char out_data[8];

    for (int i = 0; i < 64; i += 2) {
      size_t bytes_read = q.read_front(out_data, 8);
      ASSERT_EQ(bytes_read, 8u);

      in_data[0] = (char)(i & 0xFF);
      in_data[1] = in_data[0] * 3;
      in_data[2] = in_data[0] ^ in_data[1];
      in_data[3] = in_data[0] ^ in_data[2];
      in_data[4] = (char)((i + 1) & 0xFF);
      in_data[5] = in_data[4] * 3;
      in_data[6] = in_data[4] ^ in_data[5];
      in_data[7] = in_data[4] ^ in_data[6];

      ASSERT_EQ(memcmp(out_data, in_data, 8), 0);
    }
  }

  TESTCASE("write with offset replaces existing values") {
    // given
    char data[256];
    memset(data, '!', sizeof(data));

    p2::ring_buffer<256> q;
    q.write(data, sizeof(data));

    // when
    q.write("HELLO", 5, 1);

    // then
    char buf[10];
    q.read_front(buf, 10);
    ASSERT_EQ(buf[0], '!');
    ASSERT_EQ(buf[1], 'H');
    ASSERT_EQ(buf[2], 'E');
    ASSERT_EQ(buf[3], 'L');
    ASSERT_EQ(buf[4], 'L');
    ASSERT_EQ(buf[5], 'O');
    ASSERT_EQ(buf[6], '!');
    ASSERT_EQ(buf[7], '!');
  }

  TESTCASE("write with offset after existing values writes 0") {
    // given
    p2::ring_buffer<256> q;

    // when
    q.write("HELLO", 5, 4);

    // then
    ASSERT_EQ(q.size(), 9u);

    char buf[10];
    size_t bytes_read = q.read_front(buf, 15);
    ASSERT_EQ(bytes_read, 9u);

    ASSERT_EQ(buf[0], 0);
    ASSERT_EQ(buf[1], 0);
    ASSERT_EQ(buf[2], 0);
    ASSERT_EQ(buf[3], 0);
    ASSERT_EQ(buf[4], 'H');
    ASSERT_EQ(buf[5], 'E');
    ASSERT_EQ(buf[6], 'L');
    ASSERT_EQ(buf[7], 'L');
    ASSERT_EQ(buf[8], 'O');
  }

  TESTCASE("write scenario") {
    // given
    p2::ring_buffer<8> q;

    // when
    ASSERT_EQ(q.write("BB", 2, 2), true);
    ASSERT_EQ(q.write("DD", 2, 6), true);
    ASSERT_EQ(q.write("AA", 2, 0), true);
    ASSERT_EQ(q.write("CC", 2, 4), true);

    // then
    char data[9];
    ASSERT_EQ(q.read_front(data, 8u), 8u);
    ASSERT_EQ(strncmp(data, "AABBCCDD", 8), 0);
  }
}

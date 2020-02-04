#include <support/unittest.h>
#include <support/utils.h>

#include "tcp_recv_queue.h"

TESTSUITE(tcp_recv_queue) {
  TESTCASE("readable_until returns 0 when queue is empty") {
    tcp_recv_queue q;
    ASSERT_EQ(q.readable_until(), 0u);
  }

  TESTCASE("readable_until returns the size of the segment at first seq nbr") {
    // given
    char data[256];
    tcp_recv_queue q;

    // when
    q.insert(tcp_recv_segment{0, 0, 0}, data, 256);

    // then
    ASSERT_EQ(q.readable_until(), 256u);
  }

  TESTCASE("readable_until returns 0 if there's a gap to the first segment") {
    // given
    char data[256];
    tcp_recv_queue q;

    // when
    q.insert(tcp_recv_segment{0, 10, 0}, data, 256);

    // then
    ASSERT_EQ(q.readable_until(), 0u);
  }

  TESTCASE("read_one_segment reassembles segments correctly") {
    // given
    tcp_recv_queue q;
    ASSERT_EQ(q.insert(tcp_recv_segment{0, 2, 0}, "BB", 2), true);
    ASSERT_EQ(q.insert(tcp_recv_segment{0, 7, 0}, "DD", 2), true);
    ASSERT_EQ(q.insert(tcp_recv_segment{0, 0, 0}, "AA", 2), true);
    ASSERT_EQ(q.insert(tcp_recv_segment{0, 4, 0}, "CCC", 3), true);

    // when/then
    ASSERT_EQ(q.readable_until(), 2u);

    char outdata[1024];
    memset(outdata, 0, sizeof(outdata));
    char *pos = outdata;

    tcp_recv_segment segment;

    q.read_one_segment(&segment, pos, 512);
    ASSERT_EQ(segment.length, 2u);
    ASSERT_EQ(segment.seqnbr, 0u);
    pos += segment.length;

    q.read_one_segment(&segment, pos, 512);
    ASSERT_EQ(segment.length, 2u);
    ASSERT_EQ(segment.seqnbr, 2u);
    pos += segment.length;

    q.read_one_segment(&segment, pos, 512);
    ASSERT_EQ(segment.length, 3u);
    ASSERT_EQ(segment.seqnbr, 4u);
    pos += segment.length;

    q.read_one_segment(&segment, pos, 512);
    ASSERT_EQ(segment.length, 2u);
    ASSERT_EQ(segment.seqnbr, 7u);
    pos += segment.length;

    ASSERT_EQ(q.readable_until(), 9u);
    ASSERT_EQ(memcmp(outdata, "AABBCCCDD", 9), 0);
  }

  TESTCASE("read_one_segment and insert can read/write many times") {
    char bs[100];
    char as[137];
    memset(bs, 'B', sizeof(bs));
    memset(as, 'A', sizeof(as));

    // given
    tcp_recv_queue q;
    net::tcp::sequence_number seqnbr = 0;

    for (int i = 0; i < 30000; ++i) {
      ASSERT_EQ(q.insert(tcp_recv_segment{0, seqnbr, 0}, bs, sizeof(bs)), true);
      seqnbr += sizeof(bs);
      ASSERT_EQ(q.insert(tcp_recv_segment{0, seqnbr, 0}, as, sizeof(as)), true);
      seqnbr += sizeof(as);

      tcp_recv_segment segment;
      char data[200];
      memset(data, 0, sizeof(data));
      ASSERT_EQ(q.read_one_segment(&segment, data, sizeof(data)), true);
      ASSERT_EQ(segment.length, sizeof(bs));
      ASSERT_EQ(memcmp(data, bs, sizeof(bs)), 0);

      ASSERT_EQ(q.read_one_segment(&segment, data, sizeof(data)), true);
      ASSERT_EQ(segment.length, sizeof(as));
      ASSERT_EQ(memcmp(data, as, sizeof(as)), 0);
    }
  }

  TESTCASE("32 bit wrap-around: segments can be written over the border") {
    // given
    char data[1024];
    tcp_recv_queue q;
    q.reset(0xFFFFFFF0);

    // when
    tcp_recv_segment segment;
    ASSERT_EQ(q.insert(tcp_recv_segment{0, 0xFFFFFFF0, 0}, data, 20), true);
    ASSERT_EQ(q.readable_until(), 4u);
    ASSERT_EQ(q.read_one_segment(&segment, data, sizeof(data)), true);
    ASSERT_EQ(segment.seqnbr, 0xFFFFFFF0);
    ASSERT_EQ(segment.length, 20u);

    ASSERT_EQ(q.insert(tcp_recv_segment{0, 4u, 0}, data, 20), true);
    ASSERT_EQ(q.readable_until(), 24u);
    ASSERT_EQ(q.read_one_segment(&segment, data, sizeof(data)), true);
    ASSERT_EQ(segment.seqnbr, 4u);
    ASSERT_EQ(segment.length, 20u);
  }

  TESTCASE("32 bit wrap-around: segments are accepted when reader is lagging") {
    // given
    char data[1024];
    tcp_recv_queue q;
    q.reset(0xFFFFFFF0);

    // when
    ASSERT_EQ(q.insert(tcp_recv_segment{0, 0xFFFFFFF0, 0}, data, 20), true);
    ASSERT_EQ(q.readable_until(), 4u);
    ASSERT_EQ(q.insert(tcp_recv_segment{0, 4u, 0}, data, 1000), true);
  }
}

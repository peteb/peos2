#include "support/unittest.h"
#include "support/flip_buffer.h"

TESTSUITE(p2::flip_buffer) {
  TESTCASE("allocating more than _MaxLen returns an invalid handle") {
    p2::flip_buffer<1024> buf;
    auto handle = buf.alloc(1025);
    ASSERT_EQ(buf.data(handle), nullptr);
  }

  TESTCASE("can allocate the full _MaxLen") {
    p2::flip_buffer<1024> buf;
    auto handle = buf.alloc(1024);
    ASSERT_NEQ(buf.data(handle), nullptr);
  }

  TESTCASE("flipping between buffers invalidates pointers") {
    p2::flip_buffer<1024> buf;

    auto handle1 = buf.alloc(512);
    auto handle2 = buf.alloc(1024);

    ASSERT_NEQ(buf.data(handle1), nullptr);
    ASSERT_NEQ(buf.data(handle2), nullptr);

    auto handle3 = buf.alloc(1);
    ASSERT_EQ(buf.data(handle1), nullptr);
    ASSERT_NEQ(buf.data(handle2), nullptr);
    ASSERT_NEQ(buf.data(handle3), nullptr);

    auto handle4 = buf.alloc(1023);
    ASSERT_EQ(buf.data(handle1), nullptr);
    ASSERT_NEQ(buf.data(handle2), nullptr);
    ASSERT_NEQ(buf.data(handle3), nullptr);
    ASSERT_NEQ(buf.data(handle4), nullptr);

    auto handle5 = buf.alloc(256);
    ASSERT_EQ(buf.data(handle1), nullptr);
    ASSERT_EQ(buf.data(handle2), nullptr);
    ASSERT_NEQ(buf.data(handle3), nullptr);
    ASSERT_NEQ(buf.data(handle4), nullptr);
    ASSERT_NEQ(buf.data(handle5), nullptr);
  }
}

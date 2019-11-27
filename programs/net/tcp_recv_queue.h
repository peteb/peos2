// -*- c++ -*-

#ifndef NET_TCP_RECV_QUEUE_H
#define NET_TCP_RECV_QUEUE_H

#include <stddef.h>
#include <stdint.h>

#include <support/pool.h>
#include <support/ring_buffer.h>

typedef uint32_t tcp_seqnbr;

struct tcp_recv_segment {
  uint16_t flags;
  tcp_seqnbr seqnbr;
  uint32_t length;
};

// tcp_recv_queue - a queue that reassembles segments
class tcp_recv_queue {
public:
  bool insert(const tcp_recv_segment &segment, const char *data, size_t len);
  bool read_one_segment(tcp_recv_segment *segment, char *buffer, size_t len);

  // readable_until - the largest continuous chunk starting at the read cursor
  tcp_seqnbr readable_until() const;
  tcp_seqnbr read_cursor() const;
  void reset(tcp_seqnbr seqnbr);

private:
  size_t find_front_segment() const;

  p2::pool<tcp_recv_segment, 200> _segments;
  p2::ring_buffer<0xFFFF> _data_buffer;
  tcp_seqnbr _read_cursor = 0;
};

#endif // !NET_TCP_RECV_QUEUE_H

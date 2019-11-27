// -*- c++ -*-

#ifndef NET_TCP_SEND_QUEUE_H
#define NET_TCP_SEND_QUEUE_H

#include <support/ring_buffer.h>

#include "tcp_proto.h"

struct tcp_send_segment {
  uint16_t flags;
  tcp_seqnbr seqnbr;
  uint32_t length;
};

class tcp_send_queue {
public:
  tcp_send_queue();

  bool write_back(const tcp_send_segment &segment, const char *data, size_t length);
  size_t read_one_segment(tcp_send_segment *segment, char *data, size_t length);
  bool has_readable();

  void set_window(size_t wndsz);
  void ack(tcp_seqnbr new_ack_pos);
  void reset(tcp_seqnbr seqnbr);

private:
  tcp_seqnbr _ack_pos, _write_pos;
  size_t _read_pos;
  size_t _wndsz = 0xFFFF;

  p2::ring_buffer<0xFFFF> _data_buffer;
  p2::queue<tcp_send_segment, 100> _segments;
};

#endif // !NET_TCP_SEND_QUEUE_H

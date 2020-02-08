// -*- c++ -*-

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <support/pool.h>
#include <support/ring_buffer.h>

#include "tcp/definitions.h"

namespace net::tcp {
  struct tcp_recv_segment {
    uint16_t flags;
    net::tcp::sequence_number seqnbr;
    uint32_t length;
  };

  // A queue that re-assembles, re-orders and de-duplicates segments
  class receive_queue {
  public:
    bool insert(const tcp_recv_segment &segment, const char *data, size_t len);
    bool read_one_segment(tcp_recv_segment *segment, char *buffer, size_t len);

    // readable_until - the largest continuous chunk starting at the read cursor
    net::tcp::sequence_number readable_until() const;
    bool has_readable() const;
    net::tcp::sequence_number read_cursor() const;
    void reset(net::tcp::sequence_number seqnbr);

  private:
    size_t find_front_segment() const;

    p2::fixed_pool<tcp_recv_segment, 200> _segments;
    p2::ring_buffer<0xFFFF> _data_buffer;
    net::tcp::sequence_number _read_cursor = 0;
  };
}

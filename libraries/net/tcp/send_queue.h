// -*- c++ -*-

#pragma once

#include <support/ring_buffer.h>

#include "tcp/definitions.h"

namespace net::tcp {
  // TODO: clean up these intermediary segment types
  struct tcp_send_segment {
    uint16_t flags = 0;
    net::tcp::sequence_number seqnbr;
    uint32_t data_length, send_length;
  };

  class send_queue {
  public:
    send_queue();

    bool write_back(const tcp_send_segment &segment, const char *data, size_t data_length, size_t send_length);
    size_t read_one_segment(tcp_send_segment *segment, char *data, size_t length);
    bool has_readable();

    void set_window(size_t wndsz);

    // Removes all segments up until @new_ack_pos so that they won't get
    // resent
    void ack(net::tcp::sequence_number new_ack_pos);

    void reset(net::tcp::sequence_number seqnbr);
    net::tcp::sequence_number write_cursor() const { return _write_pos; }

  private:
    net::tcp::sequence_number _ack_pos, _write_pos;
    size_t _read_pos;
    size_t _wndsz = 0xFFFF;

    p2::ring_buffer<0xFFFF> _data_buffer;
    p2::queue<tcp_send_segment, 100> _segments;
  };

}

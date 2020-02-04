#include <support/limits.h>

#include "tcp_recv_queue.h"

bool tcp_recv_queue::insert(const tcp_recv_segment &segment, const char *data, size_t length)
{
  if (_segments.full())
    return false;

  if (p2::numeric_limits<net::tcp::sequence_number>::max() - _read_cursor < _data_buffer.capacity()) {
    // The window is wrapping around
    if (segment.seqnbr < _read_cursor &&
        segment.seqnbr >= _read_cursor + _data_buffer.capacity())
      return false;
  }
  else {
    if (segment.seqnbr < _read_cursor)
      return false;
  }

  if (!_data_buffer.write(data,
                          length,
                          segment.seqnbr - _read_cursor))
    return false;

  // TODO: implement protection against duplicates

  int idx = _segments.emplace_anywhere(segment);
  _segments[idx].length = length;
  return true;
}

bool tcp_recv_queue::read_one_segment(tcp_recv_segment *segment,
                                      char *buffer,
                                      size_t len)
{
  // TODO: replace with a heap
  uint16_t front_segment = find_front_segment();
  const tcp_recv_segment &seg = _segments[front_segment];
  assert(seg.seqnbr == _read_cursor);

  *segment = seg;
  size_t bytes_read = _data_buffer.read_front(buffer, p2::min<size_t>(len, seg.length));
  assert(bytes_read == seg.length && "need to read all bytes to not get mismatching seqnbrs");
  // TODO: do something about the case above
  _segments.erase(front_segment);
  _read_cursor += seg.length;

  return true;
}

net::tcp::sequence_number tcp_recv_queue::readable_until() const
{
  if (_segments.size() == 0)
    return _read_cursor;

  const tcp_recv_segment &seg = _segments[find_front_segment()];

  if (seg.seqnbr == _read_cursor) {
    return _read_cursor + seg.length;
  }

  return _read_cursor;
}

bool tcp_recv_queue::has_readable() const
{
  if (_segments.size() == 0)
    return false;

  const tcp_recv_segment &seg = _segments[find_front_segment()];
  return seg.seqnbr == _read_cursor;
}

net::tcp::sequence_number tcp_recv_queue::read_cursor() const
{
  return _read_cursor;
}

size_t tcp_recv_queue::find_front_segment() const
{
  net::tcp::sequence_number lowest_seq = p2::numeric_limits<net::tcp::sequence_number>::max();
  auto lowest_it = _segments.end();

  for (auto it = _segments.begin(); it != _segments.end(); ++it) {
    if (it->seqnbr < lowest_seq) {
      lowest_seq = it->seqnbr;
      lowest_it = it;
    }
  }

  assert(lowest_it != _segments.end() && "cannot call find_front_segment on empty queue");
  return lowest_it.index();
}

void tcp_recv_queue::reset(net::tcp::sequence_number seqnbr)
{
  _read_cursor = seqnbr;
  _data_buffer.clear();
  _segments.clear();
}

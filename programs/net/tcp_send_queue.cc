#include "tcp_send_queue.h"
#include "utils.h"

tcp_send_queue::tcp_send_queue()
{
  reset(0);
}

bool tcp_send_queue::write_back(const tcp_send_segment &segment,
                                const char *data,
                                size_t data_length,
                                size_t send_length)
{
  if (data_length > _data_buffer.capacity() - _data_buffer.size())
    return false;

  if (segment.seqnbr != _write_pos) {
    return false;
  }

  if (data)
    _data_buffer.write(data, data_length);

  tcp_send_segment segment_(segment);
  segment_.data_length = data_length;
  segment_.send_length = send_length;
  _write_pos += data_length;

  return _segments.push_back(segment_);
}

size_t tcp_send_queue::read_one_segment(tcp_send_segment *segment, char *data, size_t length)
{
  if (_segments.size() == 0)
    return 0;

  const tcp_send_segment &active_segment = _segments[_segments.begin() + _read_pos];

  // TODO: sequence number wrap-around

  // Ignore segments that are outside the window
  if (active_segment.seqnbr + active_segment.send_length > _ack_pos + _wndsz)
    return 0;

  size_t bytes_read = _data_buffer.read(data, active_segment.seqnbr - _ack_pos, p2::min<size_t>(active_segment.data_length, length));
  assert(bytes_read == active_segment.data_length && "read less than segment size");

  *segment = active_segment;
  ++_read_pos;

  return bytes_read;
}

bool tcp_send_queue::has_readable()
{
  return (size_t)_segments.size() > _read_pos;
}

void tcp_send_queue::set_window(size_t wndsz)
{
  _wndsz = wndsz;
}

void tcp_send_queue::reset(tcp_seqnbr seqnbr)
{
  _read_pos = 0;
  _ack_pos = _write_pos = seqnbr;
  _data_buffer.clear();
  _segments.clear();
  _wndsz = 0xFFFF;
}

void tcp_send_queue::ack(tcp_seqnbr new_ack_pos)
{
  log(tcp_send_queue, "received ack % 16d", new_ack_pos);

  // TODO: 32 bit wrap-around
  while (new_ack_pos > _ack_pos) {
    log(tcp_send_queue, "... we're at % 16d", _ack_pos);
    assert(_segments.size() > 0);

    const tcp_send_segment &segment = _segments.front();

    if (segment.seqnbr + segment.data_length > new_ack_pos)
      break;

    log(tcp_send_queue, "acking segment % 16d", segment.seqnbr);

    _data_buffer.consume(segment.data_length);
    _segments.pop_front();

    if (_read_pos > 0)
      --_read_pos;

    _ack_pos = segment.seqnbr + segment.data_length;
  }
}

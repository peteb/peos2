// -*- c++ -*-

#ifndef PEOS2_SUPPORT_RING_BUFFER_H
#define PEOS2_SUPPORT_RING_BUFFER_H

#include "support/queue.h"
#include "support/assert.h"
#include "support/utils.h"

// TODO: optimize structure

namespace p2 {
  // ring_buffer - pushes bytes on a circular queue
  template<size_t _MaxLen>
  class ring_buffer {
  public:
    // write_back - writes `length` number of bytes to an offset
    //
    // @offset: offset from the current read position. If -1, the
    //          current write position is used (appending)
    bool write(const char *data, size_t length, int offset = -1)
    {
      if (offset == -1)
        offset = _queue.size();

      if (offset + length > capacity())
        return false;

      for (size_t i = 0; i < length; ++i) {
        bool push_ok = _queue.replace(i + offset, data[i]);
        assert(push_ok);
      }

      return true;
    }

    size_t read_front(char *data, size_t max_length)
    {
      max_length = p2::min(max_length, size());
      size_t bytes_read = 0;

      while (bytes_read < max_length) {
        data[bytes_read++] = _queue.pop_front();
      }

      return bytes_read;
    }

    // TODO: rename, peek?
    size_t read(char *data, int start, int length)
    {
      int max_length = p2::max(p2::min<int>(start + length, size()) - start, 0);
      size_t bytes_read = 0;

      while (bytes_read < (size_t)max_length) {
        char c = _queue[_queue.begin() + start + bytes_read];
        data[bytes_read++] = c;
      }

      return bytes_read;
    }

    size_t consume(size_t max_length)
    {
      max_length = p2::min(max_length, size());
      size_t bytes_ignored = 0;

      while (bytes_ignored < max_length) {
        _queue.pop_front();
        ++bytes_ignored;
      }

      return bytes_ignored;
    }

    size_t size() const
    {
      // TODO: unify signedness
      return (size_t)_queue.size();
    }

    size_t capacity() const
    {
      return _MaxLen;
    }

    size_t remaining() const
    {
      return capacity() - size();
    }

    void clear()
    {
      _queue.clear();
    }

  private:
    p2::queue<char, _MaxLen> _queue;
  };
}

#endif // !PEOS2_SUPPORT_RING_BUFFER_H

// -*- c++ -*-

#ifndef PEOS_SUPPORT_FLIP_BUFFER_H
#define PEOS_SUPPORT_FLIP_BUFFER_H

#include <stddef.h>

namespace p2 {

template<size_t _MaxLen>
class flip_buffer {
public:
  using handle =  uint64_t;
  static constexpr size_t capacity = _MaxLen;

  flip_buffer()
  {
    _buffers[0].generation = 1000;
    _buffers[1].generation = 60000;
  }

  handle alloc(size_t length)
  {
    if (length > _MaxLen)
      return 0;

    if (length > _MaxLen - _cur_buf->size) {
      // No space, change current allocating buffer and invalidate
      // all existing handles
      flip();
      _cur_buf->reset();
    }

    handle new_ptr = create_ptr(_cur_buf == &_buffers[0] ? 1 : 2, _cur_buf->generation, _cur_buf->size);
    _cur_buf->size += length;
    return new_ptr;
  }

  char *data(handle handle_)
  {
    buffer *buf;

    if (ptr_buffer(handle_) == 1)
      buf = &_buffers[0];
    else if (ptr_buffer(handle_) == 2)
      buf = &_buffers[1];
    else
      return nullptr;


    if (ptr_generation(handle_) != buf->generation)
      return nullptr;

    if (ptr_offset(handle_) >= buf->size)
      return nullptr;

    return buf->data + ptr_offset(handle_);
  }

private:
  static inline uint8_t ptr_buffer(handle ptr)
  {
    return ptr >> 62;
  }

  static inline uint16_t ptr_generation(handle ptr)
  {
    return (ptr & 0x0000FFFF00000000) >> 32;
  }

  static inline uint32_t ptr_offset(handle ptr)
  {
    return ptr & 0x00000000FFFFFFFF;
  }

  static inline handle create_ptr(uint8_t buffer, uint16_t gen, uint32_t offset)
  {
    return ((uint64_t)buffer << 62) | ((uint64_t)gen << 32) | (uint64_t)offset;
  }

  void flip()
  {
    if (_cur_buf == &_buffers[0])
      _cur_buf = &_buffers[1];
    else
      _cur_buf = &_buffers[0];
  }

  struct buffer {
    void reset() {size = 0; ++generation; }

    size_t size = 0;
    char data[_MaxLen];
    uint16_t generation = 0;
  };

  buffer _buffers[2];
  buffer *_cur_buf = &_buffers[0];
};
}

#endif // !PEOS_SUPPORT_FLIP_BUFFER_H

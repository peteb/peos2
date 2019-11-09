// -*- c++ -*-

#ifndef PEOS2_SCREEN_PRIVATE_H
#define PEOS2_SCREEN_PRIVATE_H

#include "support/mem_range.h"

// Constants
static const int NUM_COLUMNS = 80;
static const int NUM_ROWS    = 25;

static inline uint16_t entry(uint8_t bg_color, uint8_t fg_color, char letter) {
  return ((bg_color & 0xF) << 4 | (fg_color & 0xF)) << 8 | (letter & 0x7F);
}

// Classes
class buffer {
public:
  // For front buffer
  buffer(uintptr_t start, uintptr_t end)
    : _front_buffer(true),
      _buffer(start, end)

  {
    _buffer.fill(entry(0x0, 0xF, ' '));
  }

  // For back buffers
  buffer()
    : buffer((uintptr_t)_storage, (uintptr_t)_storage + sizeof(_storage))
  {
    _front_buffer = false;
  }

  buffer(const buffer &other) = delete;
  buffer &operator =(const buffer &other) = delete;

  void print(const char *message, int count) {
    bool clear_until_end = false;

    while (*message && (count == -1 || count--)) {
      const bool newline = *message == '\n';

      if (*message == '\r') {
        if (position > 0) {
          _buffer[--position] = entry(0x0, 0xF, ' ');
        }

        ++message;
        continue;
      }

      if (newline) {
        // Move cursor to start of next line
        position += NUM_COLUMNS - position % NUM_COLUMNS;
      }

      if (position >= NUM_COLUMNS * NUM_ROWS) {
        // Move to start of line. Beware, this doesn't protect against large overruns
        position -= NUM_COLUMNS;

        // Scroll
        _buffer
          .subrange(0, NUM_COLUMNS * (NUM_ROWS - 1))
          .assign_overlap(_buffer.subrange(NUM_COLUMNS));

        if (newline) {
          // Clear the row immediately as we're not filling up the rows
          _buffer.subrange(position).fill(entry(0x0, 0xF, ' '));
        }
        else {
          // Delay clearing the rest of the buffer to minimize overwrite
          clear_until_end = true;
        }
      }

      if (!newline) {
        _buffer[position++] = entry(0x0, 0xF, *message);
      }

      ++message;
    }

    if (clear_until_end) {
      _buffer.subrange(position).fill(entry(0x0, 0xF, ' '));
    }

    update_cursor();
  }

  void assign(const buffer &other) {
    _buffer.assign_disjunct(other._buffer);
    position = other.position;
    update_cursor();
  }

  int position = 0;

private:
  void update_cursor() {
    if (_front_buffer) {
      set_cursor(position);
    }
  }

  bool _front_buffer = false;
  char _storage[2 * NUM_COLUMNS * NUM_ROWS];
  p2::mem_range<uint16_t> _buffer;
};

#endif // !PEOS2_SCREEN_PRIVATE_H

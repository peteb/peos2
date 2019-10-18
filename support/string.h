// -*- c++ -*-

#ifndef PEOS2_STRING_H
#define PEOS2_STRING_H

#include "assert.h"

namespace p2 {
  char digit_as_char(int digit, int radix);

  template<int N>
  class string {
  public:
    string() {
      assert(N > 0);
      _storage[0] = '\0';
    }

    string<N> &append(char c) {
      assert(_position < N - 1 && "reached end of fixed memory area");

      _storage[_position++] = c;
      _storage[_position + 1] = '\0';

      return *this;
    }

    string<N> &append(const char *str) {
      while (*str) {
        append(*str++);
      }

      return *this;
    }

    string<N> &append(uint64_t value, int width = -1, int radix = 10, char padding = ' ') {
      char *start_pos = &_storage[_position];

      if (width == -1) {
        // Calculate number of digits
        uint64_t value_left = value / radix;
        width = 1;

        while (value_left) {
          value_left /= radix;
          ++width;
        }
      }

      for (int i = 0; i < width; ++i) {
        append(padding);
      }

      char *pos = &_storage[_position - 1];
      while (pos >= start_pos) {
        int digit = value % radix;
        value /= radix;
        *pos-- = digit_as_char(digit, radix);

        if (!value) {
          break;
        }
      }

      return *this;
    }

    const char *str() const {
      return _storage;
    }

  private:
    char _storage[N];
    int _position = 0;
  };
}

#endif // !PEOS2_STRING_H

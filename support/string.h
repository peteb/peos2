// -*- c++ -*-

#ifndef PEOS2_STRING_H
#define PEOS2_STRING_H

#include "assert.h"

namespace p2 {
  char digit_as_char(int digit, int radix);

  template<int N>
  class fixed_string {
  public:
    fixed_string() {
      ASSERT(N > 0);
      storage[0] = '\0';
    }

    void append(char c) {
      ASSERT(position < N - 1 && "reached end of fixed memory area");

      storage[position++] = c;
      storage[position + 1] = '\0';
    }

    void append(const char *str) {
      while (*str) {
        append(*str++);
      }
    }

    void append(unsigned value, int width = -1, int radix = 10) {
      char *start_pos = &storage[position];

      if (width == -1) {
        // Calculate number of digits
        unsigned value_left = value / radix;
        width = 1;

        while (value_left) {
          value_left /= radix;
          ++width;
        }
      }

      for (int i = 0; i < width; ++i) {
        append(' ');
      }

      char *pos = &storage[position - 1];
      while (pos >= start_pos) {
        int digit = value % radix;
        value /= radix;
        *pos-- = digit_as_char(digit, radix);

        if (!value) {
          break;
        }
      }
    }

    const char *str() const {
      return storage;
    }

  private:
    char storage[N];
    int position = 0;
  };

}

#endif // !PEOS2_STRING_H

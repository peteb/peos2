// -*- c++ -*-

#ifndef PEOS2_STRING_H
#define PEOS2_STRING_H

#include <stdint.h>
#include "assert.h"

namespace p2 {
  char digit_as_char(int digit, int radix);

  // TODO: this class has two different "modes"; one when the data is
  // allocated by the class, one where we're referencing outside
  // memory. Might want to extract that into a "storage provider"
  // later if we need dynamic allocation
  template<int _MaxLen>
  class string {
  public:
    string() {
      assert(_MaxLen > 0);
      _storage_ref = _storage;
      _storage_ref[0] = '\0';
    }

    string(const string<_MaxLen> &other) {

    }

    // A nice helper for deducing _MaxLen from array length
    template<typename T>
    string(T (&data)[_MaxLen]) : _storage_ref(data) {
      assert(_MaxLen > 0);
      _storage_ref[0] = '\0';
    }

    template<typename T>
    string(T *(&data)) : _storage_ref(data) {
      assert(_MaxLen > 0);
      _storage_ref[0] = '\0';
    }

    string<_MaxLen> &append(char c) {
      assert(_position < _MaxLen - 1 && "buffer overrun");
      _storage_ref[_position++] = c;
      _storage_ref[_position] = '\0';

      return *this;
    }

    string<_MaxLen> &append(const char *str) {
      while (*str) {
        append(*str++);
      }

      return *this;
    }

    string<_MaxLen> &append(uint64_t value, int width = -1, int radix = 10, char padding = ' ') {
      char *start_pos = &_storage_ref[_position];

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

      char *pos = &_storage_ref[_position - 1];
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
      return _storage_ref;
    }

  private:
    char *_storage_ref;
    // This allocation will usually be optimized out by gcc when
    // unusued
    char _storage[_MaxLen];
    int _position = 0;
  };


  inline void _test_string() {
    // TODO: get rid of this function when we have usage of all cases
    {
      p2::string<123> hej;
    }

    {
      char buf[100];
      p2::string hej(buf);
    }

    {
      char buf[100];
      char *hello = buf;
      p2::string<100> hej(hello);
    }
  }
}

#endif // !PEOS2_STRING_H

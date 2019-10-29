// -*- c++ -*-

#ifndef PEOS2_STRING_H
#define PEOS2_STRING_H

#if __STDC_HOSTED__ == 1
#include <iostream>
#undef assert
#endif // __STDC_HOSTED__ == 1

#include <stdint.h>
#include "assert.h"
#include "utils.h"

namespace p2 {
  char digit_as_char(int digit, int radix);

  //
  // TODO: this class has two different "modes"; one when the data is
  // allocated by the class, one where we're referencing outside
  // memory. Might want to extract that into a "storage provider"
  // later if we need dynamic allocation
  // TODO: extract string building logic into its own "string_builder" class
  //
  template<int _MaxLen>
  class string {
  public:
    string() {
      assert(_MaxLen > 0);
      _storage_ref = _storage;
      _storage_ref[0] = '\0';
    }

    // A nice helper for deducing _MaxLen from array length
    template<typename T>
    string(T (&data)[_MaxLen])
      : _storage_ref(data) {
      assert(_MaxLen > 0);
      _storage_ref[0] = '\0';
    }

    /*template<typename T>
    string(T *(&data)) : _storage_ref(data) {
      assert(_MaxLen > 0);
      _storage_ref[0] = '\0';
      }*/

    string(const char *other, int max_length = _MaxLen - 1)
      : _storage_ref(_storage) {
      int count = 0;
      max_length = p2::min(max_length, _MaxLen - 1);

      while (*other) {
        if (count++ >= max_length) {
          break;
        }
        append(*other++);
      }
    }

    string(const p2::string<_MaxLen> &other)
      : _storage_ref(_storage) {
      for (int i = 0; i < other.size(); ++i) {
        append(other._storage_ref[i]);
      }
    }

    string<_MaxLen> &operator =(const string<_MaxLen> &other) {
      _position = 0;

      for (int i = 0; i < other.size(); ++i) {
        append(other._storage_ref[i]);
      }

      return *this;
    }

    string<_MaxLen> &append(char c) {
      assert(_position + 1 < _MaxLen && "buffer overrun");
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

    const char *c_str() const {
      return _storage_ref;
    }

    int size() const {
      return _position;
    }

    int capacity() const {
      return _MaxLen;
    }

    void clear() {
      _position = 0;
      _storage_ref[_position] = '\0';
    }

    char operator [](int idx) const {
      return _storage_ref[idx];
    }

    void backspace() {
      if (_position > 0) {
        _storage_ref[--_position] = '\0';
      }
    }

    template<int _Size>
    bool operator ==(const p2::string<_Size> &rhs) const {
      if (rhs.size() != size()) {
        return false;
      }

      for (int i = 0; i < size(); ++i) {
        if (_storage_ref[i] != rhs._storage_ref[i]) {
          return false;
        }
      }
      return true;
    }

#if __STDC_HOSTED__ == 1
    friend std::ostream &operator <<(std::ostream &out, const p2::string<_MaxLen> &rhs) {
      out << rhs.c_str();
      return out;
    }
#endif // __STDC_HOSTED__ == 1


  private:
    char *_storage_ref;
    // This allocation will usually be optimized out by gcc when
    // unusued
    char _storage[_MaxLen];
    int _position = 0;
  };
}

#endif // !PEOS2_STRING_H

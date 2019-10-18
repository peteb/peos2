// -*- c++ -*-
#ifndef PEOS2_FORMAT_H
#define PEOS2_FORMAT_H

#include "support/string.h"
#include "panic.h"

namespace p2 {
  template<int N>
  class format {
  public:
    format(const char *fmt) : _fmt_pos(fmt) {}

    format &operator %(const char *str) {
      expect_string();
      _storage.append(str);
      return *this;
    }

    format &operator %(uint64_t val) {
      number_traits traits = expect_number();
      _storage.append(val, traits.width, traits.radix, '0');
      return *this;
    }

    const char *str() {
      fmt_scan();  // to get any leftovers
      return _storage.str();
    }

  private:
    struct number_traits {
      int width, radix;
    };

    // Finds the next format flag
    void fmt_scan() {
      while (*_fmt_pos) {
        if (*_fmt_pos == '%') {
          ++_fmt_pos;

          if (*_fmt_pos != '%') {
            return;
          }
        }

        _storage.append(*_fmt_pos++);
      }
    }

    char fmt_expect(const char *values) {
      while (*values) {
        if (*_fmt_pos == *values++) {
          return *_fmt_pos++;
        }
      }

      panic("Unexpected value in format");
    }

    void expect_string() {
      fmt_scan();  // move up to next format flag
      // We don't have any formatting for strings, that's why it's this easy
      fmt_expect("s");
    }

    number_traits expect_number() {
      fmt_scan();

      switch (fmt_expect("lxd")) {
      case 'l':  // 64 bit
        fmt_expect("x");
        return {16, 16};

      case 'x':
        return {8, 16};

      case 'd':
        return {-1, 10};
      }

      __builtin_unreachable();
    }

    p2::string<N> _storage;
    const char *_fmt_pos;
  };

}

#endif // !PEOS2_FORMAT_H

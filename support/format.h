// -*- c++ -*-
#ifndef PEOS2_FORMAT_H
#define PEOS2_FORMAT_H

#include "support/string.h"
#include "panic.h"

namespace p2 {
  template<int _MaxLen>
  class format {
  public:
    format(const char *fmt) : _storage_ref(_storage), _fmt_pos(fmt) {}
    format(char (&data)[_MaxLen], const char *fmt) : _storage(data), _storage_ref(_storage), _fmt_pos(fmt) {}
    format(char *(&data), const char *fmt) : _storage(data), _storage_ref(_storage), _fmt_pos(fmt) {}
    format(p2::string<_MaxLen> &other, const char *fmt) : _storage_ref(other),  _fmt_pos(fmt) {}

    template<typename... _ArgsT>
    format(const char *fmt, _ArgsT... args) : format(fmt) {
      (*this)(args...);
      str();  // To output the rest of the format
    }

    template<typename... _ArgsT>
    format(char (&data)[_MaxLen], const char *fmt, _ArgsT... args) : format(data, fmt) {
      (*this)(args...);
      str();  // To output the rest of the format
    }

    template<typename T>
    format &operator()(T head) {
      return (*this) % head;
    }

    template<typename T, typename... _ArgsT>
    format &operator()(T head, _ArgsT... rest) {
      (*this) % head;
      return (*this)(rest...);
    }

    const p2::string<_MaxLen> &str() {
      fmt_scan();  // to get any leftovers
      return _storage_ref;
    }

    const p2::string<_MaxLen> &str() const {
      assert(!*_fmt_pos && "format has to be consumed before calling const str()");
      return _storage_ref;
    }

  private:
    // The % operator is a bit messy to use due to associativity;
    // sometimes it binds tighter to the arguments than wanted,
    // causing a modulus operation instead (and a lot of wasted time
    // debugging...), so prefer the `format(fmt, args...)` ctor.
    format &operator %(const char *str) {
      expect_string();
      _storage_ref.append(str);
      return *this;
    }

    format &operator %(uint64_t val) {
      number_traits traits = expect_number();
      _storage_ref.append(val, traits.width, traits.radix, '0');
      return *this;
    }


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

        _storage_ref.append(*_fmt_pos++);
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

    p2::string<_MaxLen> _storage, &_storage_ref;
    const char *_fmt_pos;
  };

  inline void _test_format() {
    // TODO: get rid of this function when we have usage of all cases
    {
      format<123> f("Hello %s");
    }

    {
      char buf[100];
      format hej(buf, "hello %s");
    }

    {
      char buf[100];
      char *ptr = buf;
      format<100> hej(ptr, "hello %s");
    }
  }
}

#endif // !PEOS2_FORMAT_H

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

    template<int _StrSz>
    format &operator %(const p2::string<_StrSz> &str) {
      expect_string();
      _storage_ref.append(str.c_str());
      return *this;
    }

    format &operator %(uint64_t val) {
      number_traits traits = expect_number();
      _storage_ref.append(val, traits.width, traits.radix, traits.fill);
      return *this;
    }


    struct number_traits {
      int width, radix;
      char fill;
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

      panic("fmt_expect: unexpected value in format");
    }

    char fmt_peek() {
      return *_fmt_pos;
    }

    char fmt_consume() {
      return *_fmt_pos++;
    }

    char fmt_peek(const char *values) {
      while (*values) {
        if (*_fmt_pos == *values++) {
          return *_fmt_pos;
        }
      }
      return 0;
    }

    int fmt_expect_positive_number() {
      char digit = fmt_peek();

      if (!isdigit(digit))
        panic("fmt_expect_positive_number: character not a digit");

      int number = fmt_consume() - '0';

      while (isdigit(fmt_peek())) {
        number = (number * 10) + (fmt_consume() - '0');
      }

      return number;
    }


    void expect_string() {
      fmt_scan();  // move up to next format flag
      // We don't have any formatting for strings, that's why it's this easy
      fmt_expect("s");
    }

    // A number format is either of the format %l, %x or %d, and can
    // be prefixed by a character and one or more digits: %02x,
    // %A20d. The fill character cannot be an 'l', 'x' or 'd'. This is
    // maybe unexpected, but I don't have the time to implement that
    // right now. TODO: implement
    number_traits expect_number() {
      number_traits traits = {-1, 10, ' '};
      fmt_scan();

      if (!fmt_peek("lxdp")) {
        traits.fill = fmt_consume();
        traits.width = fmt_expect_positive_number();
      }

      switch (fmt_expect("lxdp")) {
      case 'p':
        return {8, 16, '0'};

      case 'l':  // 64 bit TODO: remove this after updating all callsites
        fmt_expect("x");
        return {16, 16, '0'};

      case 'x':
        if (traits.width == -1)
          traits.width = 4;  // TODO: remove this after updating all callsites
        return {traits.width, 16, traits.fill};

      case 'd':
        return {traits.width, 10, traits.fill};
      }

      __builtin_unreachable();
    }

    p2::string<_MaxLen> _storage, &_storage_ref;
    const char *_fmt_pos;
  };

}

#endif // !PEOS2_FORMAT_H

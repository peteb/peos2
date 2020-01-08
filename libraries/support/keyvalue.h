// -*- c++ -*-

#ifndef PEOS2_SUPPORT_KEYVALUE_H
#define PEOS2_SUPPORT_KEYVALUE_H

#include "support/string.h"
#include "support/pool.h"

namespace p2 {
  static bool str_equal(const char *s1, const char *s2)
  {
    while (true) {
      if (*s1 != *s2)
        return false;

      if (!*s1)
        return true;

      ++s1, ++s2;
    }

    return true;
  }


  template<int N>
  class keyvalue {
  public:
    keyvalue(const char *line) : _arena(line) {parse(); }

    const char *operator [](const char *key) const
    {
      for (size_t i = 0; i < _attribs.watermark(); ++i) {
        if (!_attribs.valid(i))
          continue;

        if (str_equal(_attribs[i].name, key))
          return _attribs[i].value;
      }

      return nullptr;
    }

  private:
    struct attribute {
      const char *name, *value;
    };

    void parse()
    {
      while (*pos) {
        const char *key = parse_key();

        if (!key)
          continue;

        const char *value = parse_value();
        _attribs.emplace_anywhere(attribute{key, value});
      }
    }

    const char *parse_key()
    {
      // A key is text with no spaces followed by an =
      char *start_pos = pos;

      while (true) {
        switch (*pos) {
        case '=':
          // Mark the end of the string in the arena so we can point into it
          *pos++ = '\0';
          return start_pos;

        case ' ':
          // Spaces aren't valid, so stop after the space
          ++pos;
          return nullptr;

        case '\0':
          // Key can't be the last thing in a string
          return nullptr;

        default:
          ++pos;
        }
      }

      return nullptr;
    }

    const char *parse_value()
    {
      // A value is text with no spaces followed by a space or \0
      char *start_pos = pos;

      while (true) {
        switch (*pos) {
        case '=':
          ++pos;
          return nullptr;

        case ' ':
          *pos++ = '\0';
          [[fallthrough]];

        case '\0':
          return start_pos;

        default:
          ++pos;
        }
      }
    }

    p2::string<N> _arena;
    p2::pool<attribute, 16> _attribs;
    char *pos = &_arena[0];
  };
}

#endif // !PEOS2_SUPPORT_KEYVALUE_H

// -*- c++ -*-

// command_line.h -- functionality for parsing shell command lines

#ifndef SHELL_COMMAND_LINE_H
#define SHELL_COMMAND_LINE_H

#include <support/string.h>
#include <support/pool.h>

// TODO: write host unit tests (and extract unit test support)

class command_line {
public:
  int parse(const char *line)
  {
    // TODO: _args.clear();
    _arena = p2::string<256>(line);

    while (const char *component = parse_component()) {
      if (*component != '\0')
        _args.emplace_back(component);
    }

    return 0;
  }

  size_t num_arguments() const
  {
    return _args.watermark();
  }

  const char *argument(size_t pos) const
  {
    return _args[pos];
  }

private:
  const char *parse_component()
  {
    if (!_pos) {
      // Reached EOS
      return nullptr;
    }

    const char *component_start = _pos;
    _pos = strchr(_pos, ' ');

    if (_pos) {
      // Found a space, use it as a NUL terminator and move on to the
      // next char for EOS or next component start
      *_pos++ = '\0';
    }

    return component_start;
  }

  p2::string<256> _arena;
  char *_pos = &_arena[0];
  p2::pool<const char *, 32> _args;
};

#endif // !SHELL_COMMAND_LINE

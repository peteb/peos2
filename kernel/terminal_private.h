// -*- c++ -*-

#ifndef PEOS2_TERMINAL_PRIVATE_H
#define PEOS2_TERMINAL_PRIVATE_H

#include "screen.h"
#include "support/string.h"
#include "support/blocking_queue.h"

class terminal {
public:
  terminal(screen_buffer buffer) : _screen_buf(buffer) {}

  void on_key(uint16_t keycode)
  {
    if (keycode >= 0x7F) {
      // Special key
      return;
    }

    char buf[2] = {(char)keycode, '\0'};

    if (keycode == '\n') {
      if (_line_buffer.size() >= _line_buffer.capacity() - 1) {
        return;
      }

      _line_buffer.append((char)(keycode & 0xFF));
      screen_print(_screen_buf, buf);

      // Note: queue.push_back will switch to a waiting process
      // immediately
      size_t bytes_pushed = _input_queue.push_back(&_line_buffer[0], _line_buffer.size(), [&]() {
                              _line_buffer.clear();
                            });

      assert(bytes_pushed != 0);
    }
    else if (keycode == '\r') {
      if (_line_buffer.size() > 0) {
        screen_print(_screen_buf, "\r");
        _line_buffer.backspace();
      }
    }
    else {
      if (_line_buffer.size() >= _line_buffer.capacity() - 2) {
        return;
      }

      _line_buffer.append((char)(keycode & 0xFF));
      screen_print(_screen_buf, buf);
    }
  }

  void focus()
  {
    screen_switch_buffer(_screen_buf);
  }

  int syscall_write(const char *data, int length)
  {
    screen_print(_screen_buf, data, length);
    return length;
  }

  int syscall_read(char *data, int length)
  {
    return _input_queue.pop_front(data, length);
  }

private:
  screen_buffer _screen_buf;
  p2::string<200> _line_buffer;
  p2::blocking_data_queue<1000> _input_queue;
};

#endif // !PEOS2_TERMINAL_PRIVATE_H

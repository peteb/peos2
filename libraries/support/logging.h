// -*- c++ -*-

#ifndef PEOS2_SUPPORT_LOGGING_H
#define PEOS2_SUPPORT_LOGGING_H

#include <support/format.h>

extern char debug_out_buffer[512];
extern void _log_print(int level, const char *message);

#define log_debug(fmt, ...) do {                                                            \
  p2::format<512>(debug_out_buffer, "DEBUG " __FILE__ ":" TOSTRING(__LINE__) ": " fmt,    \
                  ##__VA_ARGS__).str();                                      \
  _log_print(7, debug_out_buffer);                                                       \
} while (0)

#define log_info(fmt, ...) do {                                                             \
  p2::format<512>(debug_out_buffer, "INFO  " __FILE__ ":" TOSTRING(__LINE__) ": " fmt,    \
                  ##__VA_ARGS__).str();                                      \
  _log_print(6, debug_out_buffer);                                                       \
} while (0)

#define log_warn(fmt, ...) do {                                                             \
  p2::format<512>(debug_out_buffer, "WARN  " __FILE__ ":" TOSTRING(__LINE__) ": " fmt,    \
                  ##__VA_ARGS__).str();                                      \
  _log_print(4, debug_out_buffer);                                                       \
} while (0)

#define log_error(fmt, ...) do {                                                            \
  p2::format<512>(debug_out_buffer, "ERROR " __FILE__ ":" TOSTRING(__LINE__) ": " fmt,    \
                  ##__VA_ARGS__).str();                                      \
  _log_print(3, debug_out_buffer);                                                       \
} while (0)

#endif // !PEOS2_SUPPORT_LOGGING_H

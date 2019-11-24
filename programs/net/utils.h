// -*- c++ -*-

#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <stddef.h>

extern char debug_out_buffer[512];

int read(int fd, char *buf, size_t length);

#define log(module, fmt, ...) {                                             \
    p2::format<512>(debug_out_buffer, TOSTRING(module) ": " fmt             \
                    __VA_OPT__(,) __VA_ARGS__);                             \
    puts(debug_out_buffer);}

#endif // !NET_UTILS_H

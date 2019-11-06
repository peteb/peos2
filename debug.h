// -*- c++ -*-

#ifndef PEOS2_DEBUG_H
#define PEOS2_DEBUG_H

#include "screen.h"

#undef STRINGIFY
#undef TOSTRING
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

extern char debug_out_buffer[128];

//
// dbg_puts - writes a line to the console
// Can only be used in interrupt handlers, etc.
//
#define dbg_puts(module, fmt, ...) {puts(p2::format(debug_out_buffer, TOSTRING(module) ": " fmt, __VA_ARGS__));}

#endif // !PEOS2_DEBUG_H

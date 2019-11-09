// -*- c++ -*-

#ifndef PEOS2_DEBUG_H
#define PEOS2_DEBUG_H

#include "screen.h"
#include "process.h"
#include "support/string.h"

#undef STRINGIFY
#undef TOSTRING
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

extern char debug_out_buffer[128];

//
// dbg_puts - writes a line to the console
// Can only be used in interrupt handlers, etc.
//
#define dbg_puts(module, fmt, ...) {p2::format<128>(debug_out_buffer, TOSTRING(module) "[%d]: " fmt, proc_current_pid(), __VA_ARGS__); puts(debug_out_buffer);}

#endif // !PEOS2_DEBUG_H

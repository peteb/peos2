// -*- c++ -*-

#ifndef PEOS2_DEBUG_H
#define PEOS2_DEBUG_H

#include "screen.h"

#undef STRINGIFY
#undef TOSTRING
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define dbg_puts(module, fmt, ...) puts(p2::format<256>("%s/%s: " fmt, TOSTRING(module), __func__,  __VA_ARGS__))

#endif // !PEOS2_DEBUG_H

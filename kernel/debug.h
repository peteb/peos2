// -*- c++ -*-

#ifndef PEOS2_DEBUG_H
#define PEOS2_DEBUG_H

#include "screen.h"
#include "process.h"
#include "support/string.h"
#include "x86.h"

#undef STRINGIFY
#undef TOSTRING
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

extern char debug_out_buffer[128];

//
// dbg_puts - writes a line to the console
// Can only be used in interrupt handlers, and be careful about threading...
// TODO: make debug_out_buffer thread_local
//
#ifndef NODEBUG
#define dbg_puts(module, fmt, ...) {                                             \
    p2::format<128>(debug_out_buffer, TOSTRING(module) "[%d]: " fmt,             \
                    proc_current_pid().value_or(-1u) __VA_OPT__(,) __VA_ARGS__); \
    puts(debug_out_buffer);}
#else
static inline void mute_fun(const char *, ...) {}
#define dbg_puts(module, fmt, ...) mute_fun(fmt __VA_OPT__(,) __VA_ARGS__)
#endif

static inline void dbg_break()
{
  asm volatile("xchg bx, bx");  // "magical breakpoint"
  outw(0xB004, 0x2000); // Bochs and older versions of QEMU
  outw(0x604, 0x2000);  // Newer versions of QEMU
  outw(0x4004, 0x3400); // Virtualbox
}

#endif // !PEOS2_DEBUG_H

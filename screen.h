// -*- c++ -*-

#ifndef PEOS2_SCREEN_H
#define PEOS2_SCREEN_H

#include <stdint.h>

#include "support/string.h"
#include "support/format.h"

void clear_screen();
void print(const char *message, int count = -1);
void set_cursor(uint16_t position);
void screen_backspace();

template<int N>      void print(const p2::string<N> &fs) {print(fs.str()); }
template<int N>      void print(p2::format<N> &fm) {print(fm.str()); }
template<typename T> void puts(T &val) {print(val); print("\n"); }

#endif // !PEOS2_SCREEN_H

// -*- c++ -*-
//
// VGA screen, abstracts VGA-specific logic like memory addresses,
// colors, etc.
//

#ifndef PEOS2_SCREEN_H
#define PEOS2_SCREEN_H

#include <stdint.h>

#include "support/string.h"
#include "support/format.h"

typedef uint16_t screen_buffer;

void print(const char *message, int count = -1);

void          screen_init();
screen_buffer screen_create_buffer();
void          screen_switch_buffer(screen_buffer buffer_handle);
void          screen_set_target_buffer(screen_buffer buffer_handle);
void          screen_print(screen_buffer buffer_handle, const char *message, int count = -1);
void          screen_seek(screen_buffer buffer_handle, uint16_t position);

template<int N>      void print(const p2::string<N> &fs) {print(fs.str()); }
template<int N>      void print(p2::format<N> &fm) {print(fm.str()); }
template<typename T> void puts(T val) {print(val); print("\n"); }

#endif // !PEOS2_SCREEN_H

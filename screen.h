// -*- c++ -*-

#ifndef PEOS2_SCREEN_H
#define PEOS2_SCREEN_H

#include <stdint.h>

void clear_screen();
void puts(const char *message);
void set_cursor(uint16_t position);

#endif // !PEOS2_SCREEN_H

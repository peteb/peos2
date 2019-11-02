// -*- c++ -*-
//
// This file implements a "canonical mode" terminal emulator that
// exposes an interface as a character device on the filesystem; reads
// and writes go to the terminal.  The other end of the terminal is
// either memory buffers or a VGA frame buffer.  Input is buffered
// until the user presses \n, then the data is readable using `read´.
//
// The emulator's responsibilities include...
//   * Writing to the correct memory locations
//   * Buffering keyboard input
//   * Handling escape sequences
//

#ifndef PEOS2_TERMINAL_H
#define PEOS2_TERMINAL_H

#include <stdint.h>

#include "screen.h"

void term_init(const char *name, screen_buffer buffer);
void term_keypress(uint16_t keycode);

#endif // !PEOS2_TERMINAL_H

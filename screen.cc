#include "screen.h"

#include "support/mem_range.h"
#include "x86.h"

using namespace p2;

// Constants
static const int NUM_COLUMNS = 80;
static const int NUM_ROWS    = 25;
static const mem_range<uint16_t> FRAMEBUFFER = {0xB8000, 0xB8000 + 2 * NUM_COLUMNS * NUM_ROWS};

// Global state
static int screen_position = 0;

// Declarations
static void show_cursor();
static void hide_cursor();

static inline uint16_t entry(uint8_t bg_color, uint8_t fg_color, char letter) {
  return ((bg_color & 0xF) << 4 | (fg_color & 0xF)) << 8 | (letter & 0x7F);
}

void clear_screen() {
  FRAMEBUFFER.fill(entry(0x0, 0xF, ' '));

  set_cursor(0);
  show_cursor();
  screen_position = 0;
}

void print(const char *message) {
  bool clear_until_end = false;

  while (*message) {
    const bool newline = *message == '\n';

    if (newline) {
      screen_position += NUM_COLUMNS - screen_position % NUM_COLUMNS;  // Move cursor to start of next line
    }

    if (screen_position >= NUM_COLUMNS * NUM_ROWS) {
      screen_position -= NUM_COLUMNS;  // Move to start of line. Beware, this doesn't protect against large overruns

      // Scroll
      FRAMEBUFFER
        .subrange(0, NUM_COLUMNS * (NUM_ROWS - 1))
        .assign_overlap(FRAMEBUFFER.subrange(NUM_COLUMNS));

      if (newline) {
        // Clear the row immediately as we're not filling up the rows
        FRAMEBUFFER.subrange(screen_position).fill(entry(0x0, 0xF, ' '));
      }
      else {
        // Delay clearing the rest of the buffer to minimize overwrite
        clear_until_end = true;
      }
    }

    if (!newline) {
      FRAMEBUFFER[screen_position++] = entry(0x0, 0xF, *message);
    }

    ++message;
  }

  if (clear_until_end) {
    FRAMEBUFFER.subrange(screen_position).fill(entry(0x0, 0xF, ' '));
  }

  set_cursor(screen_position);
}

void set_cursor(uint16_t position) {
  // 0x3D4 = CRT Controller Address Register for color mode
  // 0x3D5 = CRT Controller Data Register for color mode
  outb(0x3D4, 0x0E);  // 0x0E = Cursor location low register
  outb(0x3D5, position >> 8);
  outb(0x3D4, 0x0F);  // 0x0F = Cursor location high register
  outb(0x3D5, position);
}

void show_cursor() {
  outb(0x3D4, 0x0A);  // 0x0A = Cursor start register
  outb(0x3D5, inb(0x3D5) & ~0x10);
}

void __attribute__ ((unused)) hide_cursor() {
  outb(0x3D4, 0x0A);
  outb(0x3D5, inb(0x3D5) | 0x10);
}

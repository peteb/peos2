#include "screen.h"
#include "x86.h"
#include "memareas.h"
#include "serial.h"

#include "support/pool.h"

// Declarations
static void show_cursor();
static void hide_cursor();
static void set_cursor(uint16_t position);

// Classes etc
#include "screen_private.h"

// Global state
static buffer front_buffer(PHYS2KERNVIRT(0xB8000), PHYS2KERNVIRT(0xB8000 + 2 * NUM_COLUMNS * NUM_ROWS));
static p2::pool<buffer, 16, screen_buffer> buffers;
static screen_buffer current_buffer = buffers.end(), target_buffer = buffers.end();

void screen_init()
{
  hide_cursor();

  // Create the initial screen
  screen_buffer main_back_buffer = screen_create_buffer();
  screen_set_target_buffer(main_back_buffer);
  screen_switch_buffer(main_back_buffer);
}

screen_buffer screen_create_buffer()
{
  return buffers.emplace_back();
}

void screen_switch_buffer(screen_buffer buffer_handle)
{
  if (current_buffer == buffer_handle) {
    return;
  }

  buffer *buf = &buffers[buffer_handle];
  current_buffer = buffer_handle;
  front_buffer.assign(*buf);
  set_cursor(buf->position);
  show_cursor();
}

void screen_print(screen_buffer buffer_handle, const char *message, int count)
{
  if (count == -1)
    count = strlen(message);

  // TODO: remove hardcoded 0
  if (buffer_handle == 0) {
    com_send(message, count);
  }

  if (buffer_handle == current_buffer) {
    front_buffer.print(message, count);
  }

  buffers[buffer_handle].print(message, count);
}

void print(const char *message, int count)
{
  if (count == -1)
    count = strlen(message);

  screen_print(target_buffer, message, count);
}

void screen_set_target_buffer(screen_buffer buffer_handle)
{
  target_buffer = buffer_handle;
}

void screen_seek(screen_buffer buffer_handle, uint16_t position)
{
  buffer *buf = &buffers[buffer_handle];
  buf->position = position;

  if (buffer_handle == current_buffer) {
    front_buffer.position = position;
    set_cursor(position);
  }
}

screen_buffer screen_current_buffer()
{
  return current_buffer;
}

void set_cursor(uint16_t position)
{
  // 0x3D4 = CRT Controller Address Register for color mode
  // 0x3D5 = CRT Controller Data Register for color mode
  outb(0x3D4, 0x0E);  // 0x0E = Cursor location low register
  outb(0x3D5, position >> 8);
  outb(0x3D4, 0x0F);  // 0x0F = Cursor location high register
  outb(0x3D5, position);
}

void show_cursor()
{
  outb(0x3D4, 0x0A);  // 0x0A = Cursor start register
  outb(0x3D5, inb(0x3D5) & ~0x10);
}

void hide_cursor()
{
  outb(0x3D4, 0x0A);
  outb(0x3D5, inb(0x3D5) | 0x10);
}

#include "serial.h"
#include "x86.h"

#define COM1_PORT_BASE 0x3F8

void com_init()
{
  outb(COM1_PORT_BASE + 1, 0x00);
  outb(COM1_PORT_BASE + 3, 0x80);
  outb(COM1_PORT_BASE + 0, 0x03);  // Baud divisor low byte, 38400
  outb(COM1_PORT_BASE + 1, 0x00);
  outb(COM1_PORT_BASE + 3, 0x03);
  outb(COM1_PORT_BASE + 2, 0xC7);
  outb(COM1_PORT_BASE + 4, 0x0B);
}

static inline bool free_to_transmit()
{
  return (inb(COM1_PORT_BASE + 5) & 0x20) != 0;
}

void send_char(char c)
{
  while (!free_to_transmit());
  outb(COM1_PORT_BASE, c);
}

void com_send(const char *data, int count)
{
  for (int i = 0; i < count; ++i) {
    send_char(data[i]);
  }
}

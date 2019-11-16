#include "serial.h"
#include "x86.h"

#define COM1_PORT_BASE 0x3F8

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

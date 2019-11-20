// serial.cc - communication over UART
//
// This is a very simple driver just so we can communicate with test
// tools. Don't expect any wonders...
//
// Bochs and Qemu pretend to be terminals by sending escape sequences,
// etc, so this file adapts the VT102-esque behavior to what the rest
// of the system expects. Remember that we're not aiming for full
// terminal support; that should be implemented at another layer later.

#include "serial.h"
#include "x86.h"
#include "protected_mode.h"
#include "debug.h"
#include "terminal.h"

#define COM1_PORT_BASE 0x3F8

void com_init()
{
  outb(COM1_PORT_BASE + 3, 0x80);  // DLAB on
  outb(COM1_PORT_BASE + 0, 0x03);  // Baud divisor low byte, 38400
  outb(COM1_PORT_BASE + 1, 0x00);  // High byte
  outb(COM1_PORT_BASE + 3, 0x03);  // DLAB off, 8 bits, no parity
  outb(COM1_PORT_BASE + 2, 0xC7);  // FIFO
  outb(COM1_PORT_BASE + 4, 0x0B);
}

static inline bool free_to_transmit()
{
  return (inb(COM1_PORT_BASE + 5) & 0x20) != 0;
}

static inline bool free_to_receive()
{
  return (inb(COM1_PORT_BASE + 5) & 0x01) != 0;
}

void send_char(char c)
{
  while (!free_to_transmit());
  outb(COM1_PORT_BASE, c);
}

void com_send(const char *data, int count)
{
  for (int i = 0; i < count; ++i) {
    switch (data[i]) {
    case '\n':
      send_char('\r');
      send_char('\n');
      break;

    default:
      send_char(data[i]);
    }
  }
}

extern "C" void isr_com1(isr_registers *);

void com_setup_rx()
{
  int_register(IRQ_BASE_INTERRUPT + IRQ_COM1,
               isr_com1,
               KERNEL_CODE_SEL,
               IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);

  irq_enable(IRQ_COM1);
  outb(COM1_PORT_BASE + 1, 0x01);
}

extern "C" void int_com1(volatile isr_registers *)
{
  // TODO: check which com port actually triggered this IRQ

  while (free_to_receive()) {
    char c = (char)inb(COM1_PORT_BASE);

    switch (c) {
    case '\r':
      term_keypress('\n');
      break;

    case '\x7f':
      term_keypress('\b');
      break;

    default:
      term_keypress(c);
    }
  }

  irq_eoi(IRQ_COM1);
}

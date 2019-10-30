#include "keyboard.h"
#include "x86.h"
#include "screen.h"
#include "protected_mode.h"
#include "terminal.h"

#define KBD_DATA   0x60
#define KBD_CMD    0x64
#define KBD_STATUS 0x64

struct scancode_mapping {
  int16_t no_mod, shift_mod, ctrl_mod;
};

// Set 1 (XT) Swedish layout
static scancode_mapping scancode_set1_map[0xFF] =
{
 {-1, -1, -1},
 {'\e', '\e',   '\e'},
 { '1',  '!',     -1},
 { '2',  '"',     -1},
 { '3',  '#',     -1},
 { '4',  '$',     -1},
 { '5',  '%',     -1},
 { '6',  '&',     -1},
 { '7',  '/',     -1},
 { '8',  '(',     -1},
 { '9',  ')',     -1},
 { '0',  '=',     -1},
 { '+',  '?',     -1},
 { '`',  '`',     -1},  // TODO
 {'\r', '\r',     -1},
 {'\t', '\t',     -1},
 { 'q',  'Q', '\x11'},
 { 'w',  'W', '\x17'},
 { 'e',  'E', '\x05'},
 { 'r',  'R', '\x12'},
 { 't',  'T', '\x14'},
 { 'y',  'Y', '\x19'},
 { 'u',  'U', '\x15'},
 { 'i',  'I', '\x09'},
 { 'o',  'O',     -1},
 { 'p',  'P', '\x10'},
 { 'a',  'A',     -1},
 { '^',  '^',     -1},  // TODO
 {'\n', '\n',     -1},
 // 0x1B
 {-1, -1, -1}, // L Ctrl
 { 'a',  'A', '\x01'},
 { 's',  'S', '\x13'},
 { 'd',  'D', '\x04'},
 { 'f',  'F', '\x06'},
 { 'g',  'G', '\x07'},
 { 'h',  'H', '\x08'},
 { 'j',  'J', '\x0A'},
 { 'k',  'K', '\x0B'},
 { 'l',  'L', '\x0C'},
 { 'o',  'O', '\x0F'},
 { 'a',  'A',   -1},
 { '<',  '>',   -1},
 {-1, -1, -1},  // L Shift
 { '*',  '*',   -1}, // 0x2B
 { 'z',  'Z', '\x1A'},
 { 'x',  'X', '\x18'},
 { 'c',  'C', '\x03'},
 { 'v',  'V', '\x16'},
 { 'b',  'B', '\x02'},
 { 'n',  'N', '\x0E'},
 { 'm',  'M', '\x0D'},
 { ',',  ';',     -1},
 { '.',  ':',     -1},
 { '-',  '_',     -1},
 {-1, -1, -1},  // R Shift
 { 'A',  'A',     -1},
 {-1, -1, -1},  // Alt
 { ' ',  ' ',     -1},
 {-1, -1, -1},

 {VK_F1, VK_F1, -1},
 {VK_F2, VK_F2, -1},
 {VK_F3, VK_F3, -1},
 {VK_F4, VK_F4, -1},
 {VK_F5, VK_F5, -1},
 {VK_F6, VK_F6, -1},
 {VK_F7, VK_F7, -1},
 {VK_F8, VK_F8, -1},
 {VK_F9, VK_F9, -1},
 {VK_F10, VK_F10, -1},
 {VK_F11, VK_F11, -1},
 {-1, -1, -1},
 {-1, -1, -1},
 {VK_UP, VK_UP,  -1},
 {-1, -1, -1},
 {-1, -1, -1},
 {VK_LEFT,  VK_LEFT,  -1},
 {-1, -1, -1},
 {VK_RIGHT, VK_RIGHT,  -1},
 {-1, -1, -1},
 {-1, -1, -1},
 {VK_DOWN, VK_DOWN,  -1},
};

static bool shift_depressed = false, ctrl_depressed = false;

extern "C" void int_kbd(isr_registers regs) {
  bool gray_keys = false;
  (void)gray_keys;
  (void)regs;

  for (int i = 0; i < 4 && (inb(KBD_STATUS) & 0x01); ++i) {
    uint16_t scancode = inb(0x60);

    if (scancode == 0xE0) {
      // "Gray" keys:
      // make/repeat: E0nn
      // break: E080nn

      // Num-lock sensitive:
      // four byte make: E02AE0nn
      // two byte repeat: E0nn
      gray_keys = true;
    }
    // TODO: support num-lock sensitive codes
    else {
      bool break_code = false;

      if (scancode >= 0x80) {
        // Key was released
        break_code = true;
        scancode -= 0x80;
      }

      switch (scancode) {
      case 0x2A:
      case 0x36:
        shift_depressed = !break_code;
        break;

      case 0x1D:
        ctrl_depressed = !break_code;
        break;
      }

      if (break_code) {
        continue;
      }

      // Key code
      auto *mapped_scancode = &scancode_set1_map[scancode];

      int16_t key_code;
      if (shift_depressed) {
        key_code = mapped_scancode->shift_mod;
      }
      else if (ctrl_depressed) {
        key_code = mapped_scancode->ctrl_mod;
      }
      else {
        key_code = mapped_scancode->no_mod;
      }

      if (key_code) {
        irq_eoi(IRQ_KEYBOARD);
        term_keypress(key_code);  // Can take a while to return if a context switch happens
        return;
      }
    }
  }

  irq_eoi(IRQ_KEYBOARD);
}

extern "C" void isr_kbd(isr_registers);

void kbd_init() {
  // TODO: verify that the PS/2 controller exists using ACPI
  outb_wait(KBD_CMD, 0xAA);
  if (inb(KBD_DATA) != 0x55) {
    panic("PS/2 controller self-test failed");
  }

  // TODO: extend setup to make sure we're in a good state
  irq_enable(IRQ_KEYBOARD);
  int_register(0x20 + IRQ_KEYBOARD, isr_kbd, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P);

  puts("Initialized keyboard");
}

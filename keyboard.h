// -*- c++ -*-

#ifndef PEOS2_KEYBOARD_H
#define PEOS2_KEYBOARD_H

// ASCII keys are < 0x7F
// Special keys are > 0xFF

#define VK_F1       0x01FF
#define VK_F2       0x02FF
#define VK_F3       0x03FF
#define VK_F4       0x04FF
#define VK_F5       0x05FF
#define VK_F6       0x06FF
#define VK_F7       0x07FF
#define VK_F8       0x08FF
#define VK_F9       0x09FF
#define VK_F10      0x0AFF
#define VK_F11      0x0BFF
#define VK_F12      0x0CFF
#define VK_LEFT     0x11FF
#define VK_RIGHT    0x12FF
#define VK_UP       0x13FF
#define VK_DOWN     0x14FF

void kbd_init();

#endif // !PEOS2_KEYBOARD_H

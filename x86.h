// -*- c++ -*-

#ifndef PEOS2_X86_H
#define PEOS2_X86_H

#include <stdint.h>

#define CR0_PE 0x00000001
#define CR0_MP 0x00000002
#define CR0_EM 0x00000004
#define CR0_TS 0x00000008
#define CR0_ET 0x00000010
#define CR0_NE 0x00000020
#define CR0_WP 0x00010000
#define CR0_AM 0x00040000
#define CR0_NW 0x20000000
#define CR0_CD 0x40000000
#define CR0_PG 0x80000000

#define GDT_TYPE_P           0x80  // Segment present
#define GDT_TYPE_DPL3        0x60  // Descriptor privilege level
#define GDT_TYPE_A           0x01  // Accessed (data and code)
#define GDT_TYPE_W           0x02  // Read-write (data)
#define GDT_TYPE_E           0x04  // Expand-down (data)
#define GDT_TYPE_R           0x02  // Read-execute (code)
#define GDT_TYPE_C           0x04  // Conforming (code)
#define GDT_TYPE_CODE        0x18  // S + top bit of type
#define GDT_TYPE_DATA        0x10  // S
#define GDT_TYPE_32TSS_A     0x09  // 32 bit TSS (available)

#define GDT_FLAGS_AVL        0x01  // Available for use by system software
#define GDT_FLAGS_L          0x02  // 64-bit code segment
#define GDT_FLAGS_DB         0x04  // 0 = 16-bit segment, 1 = 32-bit
#define GDT_FLAGS_G          0x08  // Granularity. 1 = limit is increments of 4k

#define IDT_TYPE_D           0x08  // Size of gate; 1 = 32 bits, 0 = 16 bits
#define IDT_TYPE_INTERRUPT   0x06  // Interrupt gate
#define IDT_TYPE_DPL3        0x60  // Descriptor privilege level
#define IDT_TYPE_P           0x80  // Segment Present

#define PIC_MASTER_CMD     0x20
#define PIC_MASTER_DATA    0x21
#define PIC_SLAVE_CMD      0xA0
#define PIC_SLAVE_DATA     0xA1

#define PIC_CMD_INIT       0x11  // init + IC4

#define IRQ_SYSTEM_TIMER   0
#define IRQ_KEYBOARD       1
#define IRQ_CASCADE        2

#define IRQ_BASE_INTERRUPT 0x20

#define GDT_SEGSEL(rpl, index) (((rpl) & 0x3) | ((index) << 3))

struct gdt_descriptor {
  gdt_descriptor(uint32_t base, uint32_t limit, uint8_t flags, uint8_t type);

  uint16_t limit_0_15;
  uint16_t base_0_15;
  uint8_t  base_16_23;
  uint8_t  type;             // P, DPL, S, Type
  uint8_t  limit_16_19:4;
  uint8_t  flags:4;          // G, D/B, L, AVL
  uint8_t  base_24_31;
} __attribute__((packed));

struct gdtr {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed));

struct idt_descriptor {
  idt_descriptor(uint32_t offset, uint16_t segment, uint8_t type);
  idt_descriptor() = default;

  uint16_t offset_0_15;
  uint16_t segment_selector;
  uint8_t  zero;
  uint8_t  type;             // P, DPL, D, Type
  uint16_t offset_16_31;
} __attribute__((packed));

// Interrupt invoked without CPL decrease
struct int_frame_same_cpl {
  uint32_t eip;
  uint32_t cs;
  uint32_t eflags;
};

struct tss_entry {
  uint32_t prev_tss;
  uint32_t esp0;
  uint32_t ss0;
  uint32_t esp1;
  uint32_t ss1;
  uint32_t esp2;
  uint32_t ss2;
  uint32_t cr3;
  uint32_t eip;
  uint32_t eflags;
  uint32_t eax;
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t esp;
  uint32_t ebp;
  uint32_t esi;
  uint32_t edi;
  uint32_t es;
  uint32_t cs;
  uint32_t ss;
  uint32_t ds;
  uint32_t fs;
  uint32_t gs;
  uint32_t ldt;
  uint16_t trap;
  uint16_t iomap_base;
} __attribute__((packed));

inline void outb(uint16_t port, uint8_t value) {
  // dN = 8 bits are pased as immediate, while larger values are set in dx
  asm volatile("out %0, %1" : : "dN" (port), "a" (value));
}

inline void io_wait() {
  asm volatile("outb 0x80, al" : : "a"(0));
}

inline void outb_wait(uint16_t port, uint8_t value) {
  // dN = 8 bits are passed as immediate, while larger values are set in dx
  asm volatile("out %0, %1" : : "dN" (port), "a" (value));
  io_wait();
}

inline void outw(uint16_t port, uint16_t value) {
  asm volatile("out %0, %1" : : "dN" (port), "ax" (value));
}

inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  asm volatile("in %0, %1" : "=a" (ret) : "dN" (port));
  return ret;
}

void setup_interrupts();
void register_interrupt(int num, void (*handler)(int_frame_same_cpl *), uint16_t segment_selector, uint8_t type);
void init_pic();
void irq_enable(uint8_t irq_line);
void irq_disable(uint8_t irq_line);
void irq_eoi(uint8_t irq_line);
void enter_ring3(void *fun);

#endif // !PEOS2_X86_H

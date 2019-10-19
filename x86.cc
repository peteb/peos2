#include "x86.h"
#include "assert.h"
#include "screen.h"
#include "support/format.h"
#include "protected_mode.h"

struct int_frame_same_dpl {
  uint32_t eip;
  uint32_t cs;
  uint32_t eflags;
};

static idt_descriptor idt_descriptors[256] alignas(8);

template<typename T>
static void register_interrupt(int num, T func, uint16_t segment_selector, uint8_t type) {
  idt_descriptors[num] = {reinterpret_cast<uint32_t>(func), segment_selector, type};
}

gdt_descriptor::gdt_descriptor(uint32_t base,
                               uint32_t limit,
                               uint8_t flags,
                               uint8_t type) {

  assert(limit <= 0x000FFFFF && "limit can only be 20 bits");

  this->base_0_15 = base & 0xFFFF;
  this->base_16_23 = (base >> 16) & 0xFF;
  this->base_24_31 = (base >> 24) & 0xFF;
  this->limit_0_15 = limit & 0xFFFF;
  this->limit_16_19 = (limit >> 16) & 0xF;
  this->flags = flags;
  this->type = type;
}

idt_descriptor::idt_descriptor(uint32_t offset, uint16_t segment, uint8_t type) {
  this->offset_0_15 = offset;
  this->offset_16_31 = offset >> 16;
  this->segment_selector = segment;
  this->zero = 0;
  this->type = type;
}

__attribute__((interrupt)) void interrupt_debug(struct int_frame_same_dpl *frame) {
  // Note: we're not using pushad/popad to avoid leaking registers; we
  // leave everything up to gcc to sort out. Ie, definitely use
  // clobbers in inline asm!! This is probably an untenable situation,
  // but the "interrupt" attribute is too nice to pass on...
  puts(p2::format<128>("[debug trap] eip=%x, cs=%x, eflags=%x")
       % frame->eip
       % frame->cs
       % frame->eflags);
}


void setup_interrupts() {
  static const gdtr idt_ptr = {sizeof(idt_descriptors) - 1, reinterpret_cast<uint32_t>(idt_descriptors)};
  asm volatile("lidt [%0]" : : "m"(idt_ptr));
  register_interrupt(3, interrupt_debug, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P);
}

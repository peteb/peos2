#include "x86.h"
#include "assert.h"
#include "screen.h"
#include "support/format.h"
#include "protected_mode.h"

static idt_descriptor idt_descriptors[256] alignas(8);

static void pic_remap(uint8_t master_offset, uint8_t slave_offset);

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

extern "C" void int_debug(isr_registers regs) {
  puts("== DEBUG =============================");
  p2::string<256> buf;
  regs.to_string(buf);
  puts(buf);
}

extern "C" void int_gpf(isr_registers regs) {
  p2::string<256> buf;
  regs.to_string(buf);
  puts(buf);
  uint32_t eip_inst = *(char *)regs.eip & 0xFF;
  panic((p2::format<128>("General protection fault: %x ([eip]: %x)") % regs.error_code % eip_inst).str().c_str());
}

extern "C" void isr_debug(isr_registers);
extern "C" void isr_gpf(isr_registers);

void int_init() {
  static const gdtr idt_ptr = {sizeof(idt_descriptors) - 1, reinterpret_cast<uint32_t>(idt_descriptors)};
  asm volatile("lidt [%0]" : : "m"(idt_ptr));

  int_register(0x03, isr_debug,   KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(0x0D, isr_gpf,     KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P);
}

void int_register(int num, void (*handler)(isr_registers), uint16_t segment_selector, uint8_t type) {
  idt_descriptors[num] = {reinterpret_cast<uint32_t>(handler), segment_selector, type};
}

void pic_init() {
  pic_remap(IRQ_BASE_INTERRUPT, IRQ_BASE_INTERRUPT + 8);

  // Mask all IRQs
  outb_wait(PIC_MASTER_DATA, 0xFF);
  outb_wait(PIC_SLAVE_DATA, 0xFF);

  irq_enable(IRQ_CASCADE);
}

void pic_remap(uint8_t master_offset, uint8_t slave_offset) {
  // First initialize PIC #1
  outb_wait(PIC_MASTER_CMD, PIC_CMD_INIT);
  outb_wait(PIC_MASTER_DATA, master_offset);  // ICW 2
  outb_wait(PIC_MASTER_DATA, 0x4);            // ICW 3: there's a slave PIC at IRQ2
  outb_wait(PIC_MASTER_DATA, 0x01);           // ICW 4: 80x86 mode

  // Then initialize PIC #2
  outb_wait(PIC_SLAVE_CMD, PIC_CMD_INIT);
  outb_wait(PIC_SLAVE_DATA, slave_offset);    // ICW 2
  outb_wait(PIC_SLAVE_DATA, 0x2);             // ICW 3
  outb_wait(PIC_SLAVE_DATA, 0x01);            // ICW 4: 80x86 mode
}

void isr_registers::to_string(p2::string<256> &out) const volatile {
  (p2::format<256>(out,
                   "edi: %x esi: %x ebp: %x esp: %x\n"
                   "ebx: %x edx: %x ecx: %x eax: %x\n"
                   "eip: %x  cs: %x efl: %x ues: %x\n"
                   " ss: %x")
   % edi % esi % ebp    % esp
   % ebx % edx % ecx    % eax
   % eip % cs  % eflags % user_esp
   % ss).str();
}

void irq_disable(uint8_t irq_line) {
  uint16_t port;

  if (irq_line < 8) {
    port = PIC_MASTER_DATA;
  }
  else {
    port = PIC_SLAVE_DATA;
    irq_line -= 8;
  }

  outb_wait(port, inb(port) | (1 << irq_line));
}

void irq_enable(uint8_t irq_line) {
  uint16_t port;

  if (irq_line < 8) {
    port = PIC_MASTER_DATA;
  }
  else {
    port = PIC_SLAVE_DATA;
    irq_line -= 8;
  }

  outb_wait(port, inb(port) & ~(1 << irq_line));
}

void irq_eoi(uint8_t irq_line) {
  if (irq_line >= 8) {
    outb(PIC_SLAVE_CMD, 0x20);
  }

  outb(PIC_MASTER_CMD, 0x20);
}

void pit_set_phase(int hz) {
  int divisor = 1193180 / hz;
  outb(0x43, 0x36);           // Command 0x36
  outb(0x40, divisor & 0xFF); // Low byte to data port
  outb(0x40, divisor >> 8);   // High byte to data port
}

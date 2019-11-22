#include "x86.h"
#include "screen.h"
#include "protected_mode.h"

#include "support/assert.h"
#include "support/format.h"

static idt_descriptor idt_descriptors[256] alignas(8);
static uint32_t interrupt_stack[1024] alignas(16);

static void pic_remap(uint8_t master_offset, uint8_t slave_offset);

gdt_descriptor::gdt_descriptor(uint32_t base,
                               uint32_t limit,
                               uint8_t flags,
                               uint8_t type)
{
  assert(limit <= 0x000FFFFF && "limit can only be 20 bits");

  this->base_0_15 = base & 0xFFFF;
  this->base_16_23 = (base >> 16) & 0xFF;
  this->base_24_31 = (base >> 24) & 0xFF;
  this->limit_0_15 = limit & 0xFFFF;
  this->limit_16_19 = (limit >> 16) & 0xF;
  this->flags = flags;
  this->type = type;
}

idt_descriptor::idt_descriptor(uint32_t offset, uint16_t segment, uint8_t type)
{
  this->offset_0_15 = offset;
  this->offset_16_31 = offset >> 16;
  this->segment_selector = segment;
  this->zero = 0;
  this->type = type;
}

extern "C" void int_divzero(isr_registers *)
{
  panic("divison by zero");
}

extern "C" void int_debug(isr_registers *)
{
  panic("debug");
}

extern "C" void int_nmi(isr_registers *)
{
  panic("NMI");
}

extern "C" void int_overflow(isr_registers *)
{
  panic("overflow");
}

extern "C" void int_bre(isr_registers *)
{
  panic("bound-range exception");
}

extern "C" void int_invop(isr_registers *regs)
{
  panic("invalid opcode");
  (void)regs;
}

extern "C" void int_devnotavail(isr_registers *)
{
  panic("device not available");
}

extern "C" void int_invtss(isr_registers *)
{
  panic("invalid tss");
}

extern "C" void int_segnotpres(isr_registers *)
{
  panic("segment not present");
}

extern "C" void int_stacksegfault(isr_registers *)
{
  panic("stack segment fault");
}

extern "C" void int_fpe(isr_registers *)
{
  panic("x87 floating point exception");
}

extern "C" void int_align(isr_registers *)
{
  panic("alignment");
}

extern "C" void int_machine(isr_registers *)
{
  panic("machine check");
}

extern "C" void int_simdfp(isr_registers *)
{
  panic("SIMD FP");
}

extern "C" void int_virt(isr_registers *)
{
  panic("virtualization");
}

extern "C" void int_sec(isr_registers *)
{
  panic("security exception");
}

extern "C" void int_breakpoint(isr_registers *regs)
{
  puts("== BREAKPOINT =============================");
  p2::string<256> buf;
  regs->to_string(buf);
  puts(buf);
}

extern "C" void int_gpf(isr_registers *regs)
{
  panic(p2::format<128>("general protection fault (%x)", regs->error_code).str().c_str());
}

extern "C" void int_doublefault(isr_registers *regs)
{
  panic(p2::format<128>("double fault (%x)", regs->error_code).str().c_str());
}

extern "C" void int_spurious(isr_registers *)
{
  // nop
}

extern "C" void isr_divzero(isr_registers *);
extern "C" void isr_debug(isr_registers *);
extern "C" void isr_nmi(isr_registers *);
extern "C" void isr_breakpoint(isr_registers *);
extern "C" void isr_overflow(isr_registers *);
extern "C" void isr_bre(isr_registers *);
extern "C" void isr_invop(isr_registers *);
extern "C" void isr_devnotavail(isr_registers *);
extern "C" void isr_invtss(isr_registers *);
extern "C" void isr_segnotpres(isr_registers *);
extern "C" void isr_stacksegfault(isr_registers *);
extern "C" void isr_fpe(isr_registers *);
extern "C" void isr_align(isr_registers *);
extern "C" void isr_machine(isr_registers *);
extern "C" void isr_simdfp(isr_registers *);
extern "C" void isr_virt(isr_registers *);
extern "C" void isr_sec(isr_registers *);
extern "C" void isr_gpf(isr_registers *);
extern "C" void isr_doublefault(isr_registers *);
extern "C" void isr_spurious(isr_registers *);

void int_init()
{
  static const gdtr idt_ptr = {sizeof(idt_descriptors) - 1, reinterpret_cast<uint32_t>(idt_descriptors)};
  asm volatile("lidt [%0]" : : "m"(idt_ptr));

  int_register(INT_DIVZERO,        isr_divzero,       KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(INT_DEBUG,          isr_debug,         KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(INT_NMI,            isr_nmi,           KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(INT_BREAKPOINT,     isr_breakpoint,    KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(INT_OVERFLOW,       isr_overflow,      KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(INT_BRE,            isr_bre,           KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(INT_INVOP,          isr_invop,         KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(INT_DEVNOTAVAIL,    isr_devnotavail,   KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(INT_DOUBLEFAULT,    isr_doublefault,   KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P);
  int_register(INT_INVTSS,         isr_invtss,        KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(INT_SEGNOTPRES,     isr_segnotpres,    KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(INT_STACKSEGFAULT,  isr_stacksegfault, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(INT_GPF,            isr_gpf,           KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P);
  int_register(INT_FPE,            isr_fpe,           KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(INT_ALIGN,          isr_align,         KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(INT_MACHINE,        isr_machine,       KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(INT_SIMDFP,         isr_simdfp,        KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(INT_VIRT,           isr_virt,          KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(INT_SEC,            isr_sec,           KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);

  // Getting spurious interrupts on 0x27, and we don't use it now so just mute it
  int_register(IRQ_BASE_INTERRUPT + 7, isr_spurious,  KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);
  int_register(IRQ_BASE_INTERRUPT + 15, isr_spurious, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P|IDT_TYPE_DPL3);

  puts(p2::format<64>("IDT at %x", (uint32_t)idt_descriptors));

  // Prepare for any interrupts that might happen before we start multitasking
  size_t interrupt_stack_length = sizeof(interrupt_stack) / sizeof(interrupt_stack[0]);
  tss_set_kernel_stack((uint32_t)&interrupt_stack[interrupt_stack_length - 1]);
}

void int_register(int num, void (*handler)(isr_registers *), uint16_t segment_selector, uint8_t type)
{
  idt_descriptors[num] = {reinterpret_cast<uint32_t>(handler), segment_selector, type};
}

void pic_init()
{
  pic_remap(IRQ_BASE_INTERRUPT, IRQ_BASE_INTERRUPT + 8);

  // Mask all IRQs
  outb_wait(PIC_MASTER_DATA, 0xFF);
  outb_wait(PIC_SLAVE_DATA, 0xFF);

  irq_enable(IRQ_CASCADE);
}

void pic_remap(uint8_t master_offset, uint8_t slave_offset)
{
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

void isr_registers::to_string(p2::string<256> &out) const volatile
{
  p2::format<256> form("edi: %x esi: %x ebp: %x esp: %x\n"
                       "ebx: %x edx: %x ecx: %x eax: %x\n"
                       "eip: %x  cs: %x efl: %x ues: %x\n"
                       " ss: %x"
                       , edi, esi, ebp   , esp
                       , ebx, edx, ecx   , eax
                       , eip, cs , eflags, user_esp
                       , ss);

  out = form.str();
}

void irq_disable(uint8_t irq_line)
{
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

void irq_enable(uint8_t irq_line)
{
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

void irq_eoi(uint8_t irq_line)
{
  if (irq_line >= 8) {
    outb(PIC_SLAVE_CMD, 0x20);
  }

  outb(PIC_MASTER_CMD, 0x20);
}

void pit_set_phase(int hz)
{
  int divisor = 1193180 / hz;
  outb(0x43, 0x36);           // Command 0x36
  outb(0x40, divisor & 0xFF); // Low byte to data port
  outb(0x40, divisor >> 8);   // High byte to data port
}

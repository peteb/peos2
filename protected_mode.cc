#include "protected_mode.h"
#include "x86.h"
#include "assert.h"

static tss_entry tss;

static bool a20_enabled();
static void enable_a20();
static void setup_gdt();

extern "C" void load_gdt(const gdtr *gdt, uint32_t data_segsel);

void enter_protected_mode() {
  outb(0x70, inb(0x70) | 0x80);  // Disable NMIs
  asm volatile("cli");

  if (!a20_enabled()) {
    enable_a20();
  }

  setup_gdt();
}

void setup_gdt() {
  static const gdt_descriptor descriptors[] alignas(16) = {
    {0x0, 0x0, 0x0, 0x0},
    {0x0, 0x000FFFFF, GDT_FLAGS_G|GDT_FLAGS_DB, GDT_TYPE_CODE|GDT_TYPE_P|GDT_TYPE_R},
    {0x0, 0x000FFFFF, GDT_FLAGS_G|GDT_FLAGS_DB, GDT_TYPE_DATA|GDT_TYPE_P|GDT_TYPE_W},
    {0x0, 0x000FFFFF, GDT_FLAGS_G|GDT_FLAGS_DB, GDT_TYPE_CODE|GDT_TYPE_P|GDT_TYPE_DPL3|GDT_TYPE_R},
    {0x0, 0x000FFFFF, GDT_FLAGS_G|GDT_FLAGS_DB, GDT_TYPE_DATA|GDT_TYPE_P|GDT_TYPE_DPL3|GDT_TYPE_W},

    {(uint32_t)&tss, sizeof(tss), 0, GDT_TYPE_32TSS_A|GDT_TYPE_DPL3|GDT_TYPE_P}
  };

  static const gdtr gdt = {sizeof(descriptors) - 1, reinterpret_cast<uint32_t>(descriptors)};
  assert(sizeof(&descriptors[0]) == sizeof(gdt.base));

  // TODO: make the switch work when coming from real mode -- the code
  // now expects flat addressing already setup by QEMU multiboot
  load_gdt(&gdt, KERNEL_DATA_SEL);  // NOTE! It'll do a far jump to 0x08:*

  asm volatile("mov ax, %0\nltr ax" : : "i" (TSS_SEL));
  tss.ss0 = KERNEL_DATA_SEL;
  tss.cs = GDT_SEGSEL(3, 1);
  tss.ss = tss.ds = tss.es = tss.fs = tss.gs = GDT_SEGSEL(3, 2);
}

void tss_set_kernel_stack(uint32_t esp) {
  tss.esp0 = esp;
}

bool a20_enabled() {
  uint32_t cr0 = 0;
  asm("mov %0, cr0" : "=r"(cr0));

  if (cr0 & CR0_PE) {
    // Protected mode is enabled, so check whether the addresses below are aliases
    unsigned volatile *ptr1 = reinterpret_cast<unsigned *>(0x112345);
    unsigned volatile *ptr2 = reinterpret_cast<unsigned *>(0x012345);
    *ptr1 = 0x1234;
    *ptr2 = 0x4321;
    return *ptr1 != *ptr2;
  }
  else {
    panic("implement real mode support");
  }
}

void enable_a20() {
  // Fast A20
  outb(0x92, inb(0x92) | 0x2);

  // TODO: implement other alternatives of setting A20
}

#include "protected_mode.h"
#include "x86.h"
#include "assert.h"

static bool a20_enabled();
static void enable_a20();
static void setup_gdt();

void enter_protected_mode() {
  outb(0x70, inb(0x70) | 0x80);  // Disable NMIs
  asm volatile("cli");

  if (!a20_enabled()) {
    enable_a20();
  }

  setup_gdt();
}

void setup_gdt() {
  static const gdt_descriptor descriptors[] __attribute__((aligned(16))) = {
    {0x0, 0x0, 0x0, 0x0},
    {0x0, 0x000FFFFF, GDT_FLAGS_G|GDT_FLAGS_DB, GDT_TYPE_CODE|GDT_TYPE_P|GDT_TYPE_R},
    {0x0, 0x000FFFFF, GDT_FLAGS_G|GDT_FLAGS_DB, GDT_TYPE_DATA|GDT_TYPE_P|GDT_TYPE_W}
  };

  static const gdtr gdt = {sizeof(descriptors), reinterpret_cast<uint32_t>(descriptors)};
  assert(sizeof(&descriptors[0]) == sizeof(gdt.base));

  // TODO: make the switch work when coming from real mode -- the code
  // now expects flat addressing already setup by QEMU multiboot

  asm volatile("lgdt [%0]\n"                   // Change GDTR
               "jmp 0x08:update_data_segs\n"   // Far jump, this will read the GDTR and drain pipeline
               "update_data_segs:\n"
               "mov ax, 0x10\n"                // Update all data segments to point to GDT[2]
               "mov ds, ax\n"
               "mov es, ax\n"
               "mov fs, ax\n"
               "mov gs, ax\n"
               "mov ss, ax\n"
               "mov eax, cr0\n"
               "or eax, 1\n"
               "mov cr0, eax\n"                // Set protected mode bit
               : : "m" (gdt));
}


bool a20_enabled() {
  if (cr0() & CR0_PE) {
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

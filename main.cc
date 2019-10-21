#include "screen.h"
#include "x86.h"
#include "support/string.h"
#include "support/format.h"
#include "protected_mode.h"
#include "multiboot.h"
#include "panic.h"
#include "keyboard.h"

extern int kernel_end;

extern "C" void main(uint32_t multiboot_magic, multiboot_info *multiboot_hdr) {
  clear_screen();

  if (multiboot_magic != MULTIBOOT_MAGIC) {
    panic((p2::format<32>("Incorrect mb magic: %x") % multiboot_magic).str());
  }

  if (!(multiboot_hdr->flags & MULTIBOOT_INFO_MEM_MAP)) {
    panic("Expecting a memory map in the multiboot header");
  }

  // The bootloader supplies us with a memory map according to the Multiboot standard
  const char *mmap_ptr = reinterpret_cast<char *>(multiboot_hdr->mmap_addr);
  const char *const mmap_ptr_end = reinterpret_cast<char *>(multiboot_hdr->mmap_addr + multiboot_hdr->mmap_length);

  puts("Memory map:");
  uint64_t memory_available = 0;

  while (mmap_ptr < mmap_ptr_end) {
    const multiboot_mmap_entry *const mmap_entry = reinterpret_cast<const multiboot_mmap_entry *>(mmap_ptr);

    puts(p2::format<128>("%lx-%lx %s (%d bytes)")
         % mmap_entry->addr
         % (mmap_entry->addr + mmap_entry->len)
         % multiboot_memory_type(mmap_entry->type)
         % mmap_entry->len);

    if (mmap_entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
      memory_available += mmap_entry->len;
    }

    mmap_ptr += mmap_entry->size + sizeof(mmap_entry->size);
  }

  puts(p2::format<32>("Total avail mem: %d MB") % (memory_available / 1024 / 1024));

  puts("Entering protected mode...");
  enter_protected_mode();
  // TODO: setup a new stack?

  uint32_t esp = 0;
  uint16_t ss = 0;
  asm("mov %0, esp" : "=m" (esp) :);
  asm("mov %0, ss" : "=m"(ss) :);

  puts(p2::string<64>()
       .append("ESP =").append(esp, 8, 16, '0').append("\n")
       .append("SS  =").append(ss, 8, 16, '0').append("\n")
       .append("main=").append((uint64_t)&main, 8, 16, '0').append("\n")
       .append("end =").append((uint64_t)&kernel_end, 8, 16, '0'));

  setup_interrupts();
  asm volatile("int 3");  // Test debug int

  init_pic();
  init_keyboard();

  asm volatile("sti");
}

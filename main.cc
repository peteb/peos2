#include "screen.h"
#include "x86.h"
#include "support/string.h"
#include "support/format.h"
#include "protected_mode.h"
#include "multiboot.h"
#include "panic.h"
#include "keyboard.h"
#include "syscalls.h"
#include "filesystem.h"
#include "terminal.h"

extern int kernel_end;
extern char stack_top;

SYSCALL_DEF3(write, SYSCALL_NUM_WRITE, const char *, const char *, int);

extern "C" void kernel_start(uint32_t multiboot_magic, multiboot_info *multiboot_hdr) {
  clear_screen();

  if (multiboot_magic != MULTIBOOT_MAGIC) {
    panic((p2::format<32>("Incorrect mb magic: %x") % multiboot_magic).str().c_str());
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

  // x86 basic stuff setup
  puts("Entering protected mode...");
  enter_protected_mode();
  int_init();
  asm volatile("int 3");  // Test debug int

  pic_init();
  kbd_init();
  asm volatile("sti");
  syscalls_init();

  // Setup filesystem
  vfs_init();
  vfs_add_dirent(vfs_lookup("/"), "dev", vfs_create_node(VFS_DIRECTORY));
  term_init("term0");
  vfs_print();

  // User mode stuff
  tss_set_kernel_stack((uint32_t)&stack_top);  // Used during CPL 3 -> 0 ints
  enter_user_mode(USER_DATA_SEL, USER_CODE_SEL);

  asm volatile("int 3");

  // Do some testing from user mode
  SYSCALL3(write, "/dev/term0", "Hellote!", 8);

  // We can't use hlt anymore as we're in ring 3, but this place won't
  // be reached when we're multitasking
  while (true) {}
}

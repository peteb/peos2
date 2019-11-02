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
#include "process.h"
#include "memory.h"
#include "memareas.h"

extern int kernel_start, kernel_end;
extern char stack_top;

static uint32_t interrupt_stack[1024] alignas(16);

SYSCALL_DEF3(write, SYSCALL_NUM_WRITE, const char *, const char *, int);
SYSCALL_DEF3(read, SYSCALL_NUM_READ, const char *, char *, int);

void mt_test_t1() {
  SYSCALL3(write, "/dev/term0", "Hello from thread\n", 18);

  while (true) {
    static int count = 0;
    *(volatile char *)PHYS2KERVIRT(0xB8002) = 'A' + count;
    count = (count + 1) % 26;
  }
}

void mt_test_t2() {
  SYSCALL3(write, "/dev/term0", "Thread 2!\n", 10);

  while (true) {
    static int count = 0;
    *(volatile char *)PHYS2KERVIRT(0xB8004) = 'A' + count;
    count = (count + 1) % 26;
  }
}

void tio_test(int argc, char *argv[]) {
  SYSCALL3(write, "/dev/term0", "Hellote!\n", 9);
  char buf[64];
  auto fmt = (p2::format(buf, "Received %d argument(-s):\n") % argc);

  SYSCALL3(write, "/dev/term0", fmt.str().c_str(), fmt.str().size());

  for (int i = 0; i < argc; ++i) {
    SYSCALL3(write, "/dev/term0", argv[i], strlen(argv[i]));
  }

  while (true) {
    char input[240];
    int read = SYSCALL3(read, "/dev/term0", input, sizeof(input) - 1);
    input[read] = '\0';

    p2::format<sizeof(input) + 50> output("Read %d bytes: %s");
    output % read % input;
    SYSCALL3(write, "/dev/term0", output.str().c_str(), output.str().size());
  }
}

void tio_test2() {
  SYSCALL3(write, "/dev/term1", "Hello to the second terminal\n", 29);

  while (true) {
    char input[240];
    int read = SYSCALL3(read, "/dev/term1", input, sizeof(input) - 1);
    input[read] = '\0';

    p2::format<sizeof(input) + 50> output("Read %d bytes: %s");
    output % read % input;
    SYSCALL3(write, "/dev/term1", output.str().c_str(), output.str().size());
  }
}

uint32_t mt_exiting_fun() {
  for (int i = 0; i < 2; ++i) {
    p2::format<64> out("Counter reached %d...\n");
    out % i;
    SYSCALL3(write, "/dev/term0", out.str().c_str(), out.str().size());
  }

  SYSCALL3(write, "/dev/term0", "Bye bye!\n", 9);
  return 123;
}

void setup_test_program() {
  proc_create((void *)tio_test, "<--test-->");
  proc_create((void *)tio_test2, "");
  proc_create((void *)mt_test_t1, "");
  proc_create((void *)mt_test_t2, "");
  proc_create((void *)mt_exiting_fun, "");
}

extern "C" void kernel_main(uint32_t multiboot_magic, multiboot_info *multiboot_hdr) {
  screen_init();

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
  region largest_region = {0, 0};

  while (mmap_ptr < mmap_ptr_end) {
    const multiboot_mmap_entry *const mmap_entry = reinterpret_cast<const multiboot_mmap_entry *>(mmap_ptr);

    puts(p2::format<128>("%lx-%lx %s (%d bytes)")
         % mmap_entry->addr
         % (mmap_entry->addr + mmap_entry->len)
         % multiboot_memory_type(mmap_entry->type)
         % mmap_entry->len);

    if (mmap_entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
      memory_available += mmap_entry->len;

      if (mmap_entry->len > (largest_region.end - largest_region.start)) {
        largest_region.start = mmap_entry->addr;
        largest_region.end = mmap_entry->addr + mmap_entry->len;
      }
    }

    mmap_ptr += mmap_entry->size + sizeof(mmap_entry->size);
  }

  puts(p2::format<32>("Total avail mem: %d MB") % (memory_available / 1024 / 1024));

  puts(p2::format<64>("Kernel size: %d KB (ends at %x)",
                      ((uintptr_t)&kernel_end - (uintptr_t)&kernel_start) / 1024,
                      (uintptr_t)&kernel_end));

  // x86 basic stuff setup
  puts("Entering protected mode...");
  enter_protected_mode();
  int_init();
  asm volatile("int 3");  // Test debug int

  pic_init();
  kbd_init();
  syscalls_init();

  // Setup filesystem
  vfs_init();
  vfs_add_dirent(vfs_lookup("/"), "dev", vfs_create_node(VFS_DIRECTORY));
  term_init("term0", screen_current_buffer());

  for (int i = 1; i < 12; ++i) {
    term_init(p2::format<10>("term%d", i).str().c_str(), screen_create_buffer());
  }

  vfs_print();
  proc_init();

  // Prepare for any interrupts that might happen before we start multitasking
  size_t interrupt_stack_length = sizeof(interrupt_stack) / sizeof(interrupt_stack[0]);
  tss_set_kernel_stack((uint32_t)&interrupt_stack[interrupt_stack_length - 1]);

  // Adjust the memory region from which we'll allocate pages
  largest_region.start = p2::max(largest_region.start, KERVIRT2PHYS((uintptr_t)&kernel_end));
  largest_region.start = ALIGN_UP(largest_region.start, 0x1000);
  largest_region.end = ALIGN_DOWN(largest_region.end, 0x1000);
  mem_init(&largest_region, 1);

  // Overwrite the current mappings for the kernel to only include the
  // relevant parts and only at KERNEL_VIRT_BASE.
  mem_adrspc addr = mem_create_address_space();
  mem_activate_address_space(addr);


  setup_test_program();

  // Let the mayhem begin
  asm volatile("sti");
  proc_yield();

  while (true) {}
}

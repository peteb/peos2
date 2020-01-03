#include "screen.h"
#include "x86.h"
#include "protected_mode.h"
#include "multiboot.h"
#include "keyboard.h"
#include "syscalls.h"
#include "filesystem.h"
#include "terminal.h"
#include "process.h"
#include "memory.h"
#include "memareas.h"
#include "ramfs.h"
#include "debug.h"
#include "serial.h"
#include "pci.h"
#include "rtl8139.h"
#include "timer.h"

#include "syscall_decls.h"

#include "support/string.h"
#include "support/format.h"
#include "support/panic.h"

extern char stack_top;
extern "C" int init_main();

char debug_out_buffer[128];

extern void idle_main();

extern "C" void kernel_main(uint32_t multiboot_magic, multiboot_info *multiboot_hdr)
{
  com_init();
  screen_init();

  log(main, "initializing cpu");

  multiboot_header = (multiboot_info *)PHYS2KERNVIRT((uintptr_t)multiboot_hdr);

  if (multiboot_magic != MULTIBOOT_MAGIC) {
    panic(p2::format<32>("Incorrect mb magic: %x", multiboot_magic).str().c_str());
  }

  if (!(multiboot_hdr->flags & MULTIBOOT_INFO_MEM_MAP)) {
    panic("Expecting a memory map in the multiboot header");
  }

  // x86 basic stuff setup
  enter_protected_mode();
  int_init();
  pic_init();
  syscalls_init();
  timer_init(); // deps: syscalls

  asm volatile("int 3");  // Test debug int

  kbd_init();
  com_setup_rx();

  log(main, "initializing subsystems");
  mem_init();  // deps: arch
  proc_init();  // deps: mem
  pci_init();

  vfs_init();
  vfs_add_dirent(vfs_lookup("/"), "dev", vfs_create_node(VFS_DIRECTORY));

  log(main, "initializing drivers");

  term_init();  // deps: vfs
  ramfs_init();  // deps: vfs
  rtl8139_init();  // deps: pci

  log(main, "initializing init");
  proc_handle init_pid = proc_create(PROC_USER_SPACE|PROC_KERNEL_ACCESSIBLE, (uintptr_t)init_main);

  const char *args[] = {nullptr};
  proc_setup_user_stack(init_pid, 0, args);
  proc_enqueue(init_pid);
  proc_run();
}

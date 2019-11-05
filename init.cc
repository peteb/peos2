#include "syscall_decls.h"
#include "multiboot.h"
#include "screen.h"
#include "memareas.h"

#include "support/format.h"
#include "support/tar.h"

extern multiboot_info *multiboot_header;
static int stdout;

template<int N>
static void puts_sys(int fd, const p2::format<N> &fm) {
  SYSCALL3(write, fd, fm.str().c_str(), fm.str().size());
}

static void setup_multiboot_files();

//
// Kernel kicks off execution of this user space program "as soon as
// possible", which is after the kernel and all the drivers have been
// initialized.
//
// The init process has full access to the kernel's memory so that it
// can execute support functions.
//
extern "C" int init_main() {
  // Setup
  stdout = SYSCALL2(open, "/dev/term0", 0);

  load_multiboot_modules();
  load_archive_modules();

  {
    // Try to read a file
    int read_fd = SYSCALL2(open, "/ramfs/modules/test.tar", 0);
    char buf[1000];

    int bytes_read;
    while ((bytes_read = SYSCALL3(read, read_fd, buf, sizeof(buf) - 1)) > 0) {
      buf[p2::min(bytes_read, 10)] = '\0';

      puts_sys(stdout, p2::format<1010>("Read %d bytes: '%s'\n", bytes_read, buf));
    }
  }

  // Start I/O
  int stdin = SYSCALL2(open, "/dev/term0", 0);
  char input[240];
  int read = SYSCALL3(read, stdin, input, sizeof(input) - 1);
  input[read] = '\0';

  p2::format<sizeof(input) + 50> output("Read %d bytes: %s");
  output % read % input;
  SYSCALL3(write, stdout, output.str().c_str(), output.str().size());

  return 0;
}

static int write_info(const char *path, const char *filename, uintptr_t start_addr, uintptr_t size) {
  int mod_fd = SYSCALL2(open, p2::format<64>("/ramfs%s%s", path, filename).str().c_str(), FLAG_OPEN_CREATE);
  assert(mod_fd);
  SYSCALL4(control, mod_fd, CTRL_RAMFS_SET_FILE_RANGE, start_addr, size);
  // TODO: check return status
  // TODO: close file
  return 0;
}

void load_multiboot_modules() {
  multiboot_info *mbhdr = multiboot_header;
  multiboot_mod *modules = (multiboot_mod *)PHYS2KERVIRT(mbhdr->mods_addr);

  for (uint32_t i = 0; i < mbhdr->mods_count; ++i) {
    write_info("/modules/", (const char *)PHYS2KERVIRT(modules[i].string_addr), PHYS2KERVIRT(modules[i].mod_start), modules[i].mod_end - modules[i].mod_start);
  }
}

void load_archive_modules() {
  // TODO: enumerate files in /modules/, look for files ending in .tar
  int mod_fd = SYSCALL2(open, "/ramfs/modules/test.tar", 0);

  // TODO: read header (with loop)
  // TODO: after header, get position + add file range, seek size, loop

  // TODO: close handle
}

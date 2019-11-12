#include "syscall_decls.h"
#include "multiboot.h"
#include "screen.h"
#include "memareas.h"
#include "memory.h"
#include "tar.h"
#include "process.h"
#include "support/result.h"
#include "support/format.h"

extern multiboot_info *multiboot_header;
static int stdout;

template<int N>
static void puts_sys(int fd, const p2::format<N> &fm)
{
  syscall3(write, fd, fm.str().c_str(), fm.str().size());
}

static int verify(int retval)
{
  if (retval >= 0)
    return retval;

  // TODO: support negative values in format
  puts_sys(stdout, p2::format<32>("call failed with code %d", (uintptr_t)retval));
  syscall1(exit, 1);
  return 1;
}

static void load_multiboot_modules();
static void extract_tar(const char *filename);

//
// Kernel kicks off execution of this user space program "as soon as
// possible", which is after the kernel and all the drivers have been
// initialized.
//
// The init process has full access to the kernel's memory so that it
// can execute support functions.
//
extern "C" int init_main()
{
  // Setup
  stdout = verify(syscall2(open, "/dev/term0", 0));

  load_multiboot_modules();

  // TODO: enumerate files in /ramfs/modules and execute extract_tar
  // on those ending with .tar
  extract_tar("/ramfs/modules/init.tar");

  {
    // Try to read a file
    /*int read_fd = verify(syscall2(open, "/ramfs/bin/panic.h", 0));
    char buf[2];
    int bytes_read;

    while ((bytes_read = verify(syscall3(read, read_fd, buf, sizeof(buf)))) > 0) {
      //verify(syscall3(write, 0, buf, bytes_read));
    }

    verify(syscall1(close, read_fd));*/
  }

  /*int mmap_fd = verify(syscall2(open, "/ramfs/x86.cc", 0));
  verify(syscall5(mmap, (void *)0xBEEF0000, (void *)0xBEEFF000, mmap_fd, 0, 0));

  char *data = (char *)0xBEEF0000;
  syscall3(write, stdout, data, 4100);

  */

  // Start process
  const char *argv[] = {
    "hello",
    "param2",
    nullptr
  };

  verify(syscall2(exec, "/ramfs/bin/first_program", argv));

  // Start I/O
  int stdin = verify(syscall2(open, "/dev/term0", 0));
  char input[240];
  int read = verify(syscall3(read, stdin, input, sizeof(input) - 1));
  input[read] = '\0';

  p2::format<sizeof(input) + 50> output("Read %d bytes: %s", read, input);
  verify(syscall3(write, stdout, output.str().c_str(), output.str().size()));

  return 0;
}

static int write_info(const char *path, const char *filename, uintptr_t start_addr, uintptr_t size)
{
  int mod_fd = verify(syscall2(open, p2::format<64>("/ramfs%s%s", path, filename).str().c_str(), OPEN_CREATE));
  verify(syscall4(control, mod_fd, CTRL_RAMFS_SET_FILE_RANGE, start_addr, size));
  verify(syscall1(close, mod_fd));
  return 0;
}

void load_multiboot_modules()
{
  multiboot_info *mbhdr = multiboot_header;
  multiboot_mod *modules = (multiboot_mod *)PHYS2KERNVIRT(mbhdr->mods_addr);
  verify(syscall1(mkdir, "/ramfs/modules"));

  for (uint32_t i = 0; i < mbhdr->mods_count; ++i) {
    write_info("/modules/", (const char *)PHYS2KERNVIRT(modules[i].string_addr), PHYS2KERNVIRT(modules[i].mod_start), modules[i].mod_end - modules[i].mod_start);
  }
}

void extract_tar(const char *filename)
{
  const p2::string<6> expected_magic("ustar");
  int fd = verify(syscall2(open, filename, 0));

  tar_entry hdr;
  assert(sizeof(hdr) == 512);

  uint32_t file_mem_start = 0, file_mem_size = 0;
  verify(syscall4(control, fd, CTRL_RAMFS_GET_FILE_RANGE, (uintptr_t)&file_mem_start, (uintptr_t)&file_mem_size));
  // TODO: looping read
  while (verify(syscall3(read, fd, (char *)&hdr, sizeof(hdr))) == sizeof(hdr)) {
    if (hdr.name[0] == '\0') {
      // NUL block, two of these symbolize end of archive
      continue;
    }

    // TODO: do the string comparison without copying the string first
    if (p2::string<6>(hdr.magic, sizeof(hdr.magic)) != expected_magic) {
      panic("incorrect tar magic");
    }

    if (hdr.name[99] != '\0') {
      panic("not null-terminated name");
    }

    p2::string<101> entry_name(hdr.name, 100);
    uint32_t size = tar_parse_octal(hdr.size, sizeof(hdr.size));

    int cur_pos = 0;
    verify(syscall2(tell, fd, &cur_pos));

    if (hdr.type == '0' || hdr.type == '\0') {
      // If we can't rely on continuous memory anymore, change this into a write
      write_info("/", entry_name.c_str(), file_mem_start + cur_pos, size);
    }
    else if (hdr.type == '5') {
      char buf[128];
      p2::format(buf, "/ramfs/%s", entry_name.c_str());
      verify(syscall1(mkdir, buf));
    }

    verify(syscall3(seek, fd, ALIGN_UP(size, 512), SEEK_CUR));
  }

  verify(syscall1(close, fd));
}

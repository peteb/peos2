#include "syscall_decls.h"
#include "multiboot.h"
#include "screen.h"
#include "memareas.h"
#include "memory.h"
#include "tar.h"
#include "process.h"
#include "support/result.h"
#include "support/format.h"
#include "support/keyvalue.h"

extern multiboot_info *multiboot_header;
static int kernout;

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
  puts_sys(0, p2::format<128>("call failed with code %d", (uintptr_t)retval));
  syscall1(exit, 1);
  return 1;
}

static void load_multiboot_modules();
static void extract_tar(const char *filename);
static void setup_std_fds(const char *stdout_filename);

//
// Kernel kicks off execution of this user space program "as soon as
// possible", which is after the kernel and all the drivers have been
// initialized.
//
// The init process has full access to the kernel's memory so that it
// can execute support functions.
//
extern "C" int init_main(int argc, const char **argv)
{
  // Setup
  kernout = verify(syscall2(open, "/dev/term0", 0));
  //  puts_sys(kernout, p2::format<64>("argc: %x %s", (uint32_t)argc, argv[0]));
  //while (true) {}


  (void)argc;
  (void)argv;

  load_multiboot_modules();

  // TODO: enumerate files in /ramfs/modules and execute extract_tar
  // on those ending with .tar
  extract_tar("/ramfs/modules/init.tar");

  // TODO: launch whatever's on the command line
  const char *command_line = (const char *)PHYS2KERNVIRT(multiboot_header->cmd_line);
  assert(command_line);

  p2::keyvalue<256> attributes(command_line);
  const char *init_command = attributes["init"];
  assert(init_command);

  puts_sys(kernout, p2::format<256>("executing '%s'...\n", init_command));

  int child_pid = syscall0(fork);

  if (child_pid == 0) {
    setup_std_fds("/dev/term0");
    const char *argv[] = {init_command, nullptr};
    verify(syscall2(exec, init_command, argv));
  }
  else {
    syscall1(wait, child_pid);
    syscall0(shutdown);
  }

  return 0;
}

static int write_info(const char *path, const char *filename, uintptr_t start_addr, uintptr_t size)
{
  puts_sys(kernout, p2::format<128>("writing info to /ramfs%s%s\n", path, filename));

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

  puts_sys(kernout, p2::format<64>("mods_count: %d\n", (uint32_t)mbhdr->mods_count));
  puts_sys(kernout, p2::format<128>("cmd_line: \"%s\"\n", (const char *)PHYS2KERNVIRT(mbhdr->cmd_line)));

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

static void setup_term_at(const char *filename, int dest_fd)
{
  int source_fd = verify(syscall2(open, filename, OPEN_RETAIN_EXEC));

  if (source_fd != dest_fd) {
    verify(syscall2(dup2, source_fd, dest_fd));
    verify(syscall1(close, source_fd));
  }
}

static void setup_std_fds(const char *stdout_filename)
{
  setup_term_at(stdout_filename, 0);
  setup_term_at(stdout_filename, 1);
  setup_term_at(stdout_filename, 2);
}

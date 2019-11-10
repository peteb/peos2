#include "elf.h"

#include <stdint.h>

#include "syscall_decls.h"
#include "assert.h"
#include "debug.h"
#include "filesystem.h"

#define EI_NIDENT 16

struct elf_header {
  uint8_t  e_ident[EI_NIDENT];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint32_t e_entry;
  uint32_t e_phoff;
  uint32_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
} __attribute__((packed));

// Section header not needed for loading
struct elf_section_header {
  uint32_t sh_name;
  uint32_t sh_type;
  uint32_t sh_flags;
  uint32_t sh_addr;
  uint32_t sh_offset;
  uint32_t sh_size;
  uint32_t sh_link;
  uint32_t sh_info;
  uint32_t sh_addralign;
  uint32_t sh_entsize;
} __attribute__((packed));

#define PT_NULL               0
#define PT_LOAD               1
#define PT_DYNAMIC            2
#define PT_INTERP             3
#define PT_NOTE               4
#define PT_SHLIB              5
#define PT_PHDR               6
#define PT_LOPROC    0x70000000
#define PT_HIPROC    0x7fffffff

struct elf_program_header {
  uint32_t p_type;
  uint32_t p_offset;
  uint32_t p_vaddr;
  uint32_t p_paddr;
  uint32_t p_filesz;
  uint32_t p_memsz;
  uint32_t p_flags;
  uint32_t p_align;
} __attribute__((packed));

int elf_map_process(proc_handle pid, const char *filename)
{
  elf_header hdr = {};

  // TODO: flags
  int fd = syscall2(open, filename, 0);
  assert(fd >= 0);

  {
    // TODO: how do we handle if this would block?
    // TODO: repeating read
    int bytes_read = syscall3(read, fd, (char *)&hdr, sizeof(hdr));  // TODO: read more if 0 < read < sizeof(header)
    assert(bytes_read == sizeof(hdr));  // TODO: correct error handling
  }

  if (hdr.e_ident[0] != 0x7F ||
      hdr.e_ident[1] != 'E' ||
      hdr.e_ident[2] != 'L' ||
      hdr.e_ident[3] != 'F') {
    // TODO: error handling
    panic("not elf format");
  }

  if (hdr.e_ident[4] != 1)
    panic("only 32 bit supported");

  if (hdr.e_ident[5] != 1)
    panic("only little endian supported");

  if (hdr.e_ident[6] != 1)
    panic("only version 1 supported");

  if (hdr.e_type != 2)
    panic("Only executable files supported");

  dbg_puts(elf, "e_type: %x", hdr.e_type);
  dbg_puts(elf, "e_entry: %x", hdr.e_entry);
  dbg_puts(elf, "e_phoff: %x", hdr.e_phoff);
  dbg_puts(elf, "e_shoff: %x", hdr.e_shoff);
  dbg_puts(elf, "e_flags: %x", hdr.e_flags);
  dbg_puts(elf, "e_ehsize: %x", hdr.e_ehsize);
  dbg_puts(elf, "e_phnum: %x", hdr.e_phnum);
  dbg_puts(elf, "e_shnum: %x", hdr.e_shnum);
  dbg_puts(elf, "e_shstrndx: %x", hdr.e_shstrndx);

  elf_program_header pht[32];
  assert(hdr.e_phnum < ARRAY_SIZE(pht));

  {
    // TODO: repeating read
    size_t bytes_read = syscall3(read, fd, (char *)pht, sizeof(elf_program_header) * hdr.e_phnum);
    assert(bytes_read == sizeof(elf_program_header) * hdr.e_phnum);
  }

  syscall1(close, fd);

  // So we've managed to parse the ELF file a bit to get the program
  // header table. Let's set up the file descriptor in the target
  // process.
  p2::res<proc_fd_handle> img_open_res = vfs_syscall_open(pid, filename, 0);  // TODO: flags

  if (!img_open_res)
    return img_open_res.error();

  proc_fd_handle proc_fd = *img_open_res;

  for (int i = 0; i < hdr.e_phnum; ++i) {
    dbg_puts(elf, "type: %x ofs: %x virt: %x flags: %x",
             pht[i].p_type,
             pht[i].p_offset,
             pht[i].p_vaddr,
             pht[i].p_flags);

    if (pht[i].p_type == PT_LOAD) {
      mem_map_fd(proc_get_space(pid),
                 ALIGN_DOWN(pht[i].p_vaddr, 0x1000),
                 ALIGN_UP(pht[i].p_vaddr + pht[i].p_memsz, 0x1000),
                 proc_fd,
                 pht[i].p_offset,
                 pht[i].p_filesz,
                 MEM_AREA_USER|MEM_AREA_READWRITE);  // TODO: flags
    }
  }

  proc_setup_stack(pid, (void *)hdr.e_entry);

  return 0;
}

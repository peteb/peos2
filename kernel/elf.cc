#include "elf.h"

#include <stdint.h>

#include "syscall_decls.h"
#include "assert.h"
#include "debug.h"
#include "filesystem.h"

#define EI_NIDENT 16

#define PF_X 1
#define PF_W 2
#define PF_R 4

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
  p2::res<vfs_fd> open_result = vfs_open(proc_get_file_context(pid), filename, 0);
  if (!open_result)
    return open_result.error();

  int fd = *open_result;

  {
    // TODO: how do we handle if this would block?
    // TODO: repeating read
    p2::res<size_t> read_result = vfs_read(proc_get_file_context(pid),
                                           fd,
                                           (char *)&hdr,
                                           sizeof(hdr));

    if (!read_result)
      return read_result.error();
    else
      assert(*read_result == sizeof(hdr));  // TODO: correct error handling
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

  dbg_puts(elf, "e_entry: %x", hdr.e_entry);

  elf_program_header pht[32];
  assert(hdr.e_phnum < ARRAY_SIZE(pht));

  {
    // TODO: repeating read
    p2::res<size_t> read_result = vfs_read(proc_get_file_context(pid),
                                           fd,
                                           (char *)pht,
                                           sizeof(elf_program_header) * hdr.e_phnum);

    if (!read_result)
      return read_result.error();

    size_t bytes_read = *read_result;
    assert(bytes_read == sizeof(elf_program_header) * hdr.e_phnum);
  }

  for (int i = 0; i < hdr.e_phnum; ++i) {
    dbg_puts(elf, "type: %x ofs: %x virt: %x flags: %x",
             pht[i].p_type,
             pht[i].p_offset,
             pht[i].p_vaddr,
             pht[i].p_flags);

    if (pht[i].p_type == PT_LOAD) {
      uint16_t flags = MEM_AREA_USER;

      if (pht[i].p_flags & PF_X)
        flags |= MEM_AREA_EXECUTABLE;

      if (pht[i].p_flags & PF_W)
        flags |= MEM_AREA_READWRITE;

      if (pht[i].p_flags & PF_R)
        flags |= MEM_AREA_SYSCALL;

      mem_map_fd(proc_get_space(pid),
                 ALIGN_DOWN(pht[i].p_vaddr, 0x1000),
                 ALIGN_UP(pht[i].p_vaddr + pht[i].p_memsz, 0x1000),
                 fd,
                 pht[i].p_offset,
                 pht[i].p_filesz,
                 flags);
    }
  }

  proc_set_syscall_ret(pid, hdr.e_entry);
  return 0;
}

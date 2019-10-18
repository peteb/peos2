// -*- c++ -*-

#ifndef PEOS2_MULTIBOOT_H
#define PEOS2_MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_MAGIC         0x2BADB002
#define MULTIBOOT_INFO_MEM_MAP  0x00000040

#define MULTIBOOT_MEMORY_AVAILABLE              1
#define MULTIBOOT_MEMORY_RESERVED               2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE       3
#define MULTIBOOT_MEMORY_NVS                    4
#define MULTIBOOT_MEMORY_BADRAM                 5

struct multiboot_elf_section_hdr_table {
  uint32_t num;
  uint32_t size;
  uint32_t addr;
  uint32_t shndx;
};

struct multiboot_info {
  uint32_t flags;
  uint32_t mem_lower;
  uint32_t mem_upper;
  uint32_t boot_device;
  uint32_t cmd_line;
  uint32_t mods_count;
  uint32_t mods_addr;
  multiboot_elf_section_hdr_table elf_section_header;
  uint32_t mmap_length;
  uint32_t mmap_addr;
};

struct multiboot_mmap_entry {
  uint32_t size;
  uint64_t addr;
  uint64_t len;
  uint32_t type;
} __attribute__((packed));

const char *multiboot_memory_type(uint32_t type);

#endif // !PEOS2_MULTIBOOT_H

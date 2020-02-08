// -*- c++ -*-

#ifndef PEOS2_MEMORY_PRIVATE_H
#define PEOS2_MEMORY_PRIVATE_H

#include "support/pool.h"

#define AREA_LINEAR_MAP 1
#define AREA_ALLOC      2
#define AREA_FILE       3

#define MEM_PE_P   0x0001  // Present
#define MEM_PE_RW  0x0002  // Read/write
#define MEM_PE_U   0x0004  // User/supervisor (0 = user not allowed)
#define MEM_PE_W   0x0008  // Write through
#define MEM_PE_D   0x0010  // Cache disabled
#define MEM_PE_A   0x0020  // Accessed

#define MEM_PDE_S  0x0080  // Page size (0 = 4kb)

#define MEM_PTE_D  0x0040  // Dirty
#define MEM_PTE_G  0x0080  // Global

// Structs
struct page_dir_entry {
  uint16_t flags:12;
  uint32_t table_11_31:20;
} __attribute__((packed));

struct page_table_entry {
  uint16_t flags:12;
  uint32_t frame_11_31:20;
} __attribute__((packed));

struct area_info {
  uintptr_t start, end;
  uint16_t type, flags;
  uint16_t info_handle;
};

struct linear_map_info {
  uintptr_t phys_start;
};

struct file_map_info {
  int fd;
  uint32_t offset;
  uint32_t size;
};

struct space_info {
  space_info(page_dir_entry *page_dir) : page_dir(page_dir) {}
  page_dir_entry *page_dir;
  p2::fixed_pool<area_info, 32> areas;
  p2::fixed_pool<linear_map_info, 16> linear_maps;
  p2::fixed_pool<file_map_info, 16> file_maps;
};

#endif // !PEOS2_MEMORY_PRIVATE_H

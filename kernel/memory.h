// -*- c++ -*-
//
// Responsible for paging, page allocation, etc.
//

#ifndef PEOS2_MEMORY_H
#define PEOS2_MEMORY_H

#include <stdint.h>
#include <stddef.h>

#include "support/optional.h"

#define ALIGN_UP(val, align) (((val) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(val, align) ((val) & ~((align) - 1))

#define MEM_AREA_READWRITE       0x0001  // Area can be read and written
#define MEM_AREA_EXECUTABLE      0x0002  // Area can execute code
#define MEM_AREA_USER            0x0004  // Area can be accessed by user space
#define MEM_AREA_CACHE_DISABLED  0x0008  // No caching of pages
#define MEM_AREA_GLOBAL          0x0010  // Area will be mapped "globally"
#define MEM_AREA_SYSCALL         0x0100  // Area is good for syscall pointers
#define MEM_AREA_RETAIN_EXEC     0x0200  // Keep the area through exec

struct region {
  uintptr_t start, end;
};

typedef uint16_t mem_space;
typedef uint16_t mem_area;

void       mem_init(const region *phys_region);

// Address space management
mem_space  mem_create_space();
void       mem_destroy_space(mem_space space);
void       mem_activate_space(mem_space space);
void       mem_unmap_not_matching(mem_space space, uint16_t flags);

//
// mem_map_kernel - directly maps the kernel's memory into the space.
// @space: which address space
// @flags: MEM_AREA_* flags
//
// Will mask the READWRITE and EXECUTABLE flags appropriately for
// different memory segments.
//
void mem_map_kernel(mem_space space, uint16_t flags);
mem_area mem_map_linear(mem_space space, uintptr_t start, uintptr_t end, uintptr_t phys_start, uint16_t flags);
mem_area mem_map_linear_eager(mem_space space, uintptr_t start, uintptr_t end, uintptr_t phys_start, uint16_t flags);
mem_area mem_map_alloc(mem_space space, uintptr_t start, uintptr_t end, uint16_t flags);
mem_area mem_map_fd(mem_space space, uintptr_t start, uintptr_t end, int fd, uint32_t offset, uint32_t file_size, uint16_t flags);

p2::opt<uint16_t> mem_area_flags(mem_space space, const void *address);

#endif // !PEOS2_MEMORY_H

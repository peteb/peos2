// -*- c++ -*-
//
// Responsible for paging, page allocation, etc.
//

#ifndef PEOS2_MEMORY_H
#define PEOS2_MEMORY_H

#include <stdint.h>
#include <stddef.h>

#include "support/optional.h"
#include "support/result.h"

#define MEM_AREA_READWRITE       0x0001  // Area can be read and written
#define MEM_AREA_EXECUTABLE      0x0002  // Area can execute code
#define MEM_AREA_USER            0x0004  // Area can be accessed by user space
#define MEM_AREA_CACHE_DISABLED  0x0008  // No caching of pages
#define MEM_AREA_GLOBAL          0x0010  // Area will be mapped "globally"
#define MEM_AREA_SYSCALL         0x0100  // Area is good for syscall pointers
#define MEM_AREA_RETAIN_EXEC     0x0200  // Keep the area through exec
#define MEM_AREA_NO_FORK         0x0400  // Don't fork this area

struct region {
  uintptr_t start, end;
};

typedef uint16_t mem_space;
typedef uint16_t mem_area;

void       mem_init();

// Address space management
mem_space          mem_create_space();
void               mem_destroy_space(mem_space space);
void               mem_activate_space(mem_space space);
void               mem_unmap_not_matching(mem_space space, uint16_t flags);
p2::res<mem_space> mem_fork_space(mem_space space_handle);
void               mem_print_space(mem_space space_handle);
uintptr_t          mem_page_dir(mem_space space_handle);
void               mem_set_current_space(mem_space space_handle);

//
// mem_map_kernel - directly maps the kernel's memory into the space.
// @space: which address space
// @flags: MEM_AREA_* flags
//
// Will mask the READWRITE and EXECUTABLE flags appropriately for
// different memory segments.
//
void     mem_map_kernel(mem_space space, uint16_t flags);
mem_area mem_map_linear(mem_space space, uintptr_t start, uintptr_t end, uintptr_t phys_start, uint16_t flags);
mem_area mem_map_linear_eager(mem_space space, uintptr_t start, uintptr_t end, uintptr_t phys_start, uint16_t flags);
mem_area mem_map_alloc(mem_space space, uintptr_t start, uintptr_t end, uint16_t flags);
mem_area mem_map_fd(mem_space space, uintptr_t start, uintptr_t end, int fd, uint32_t offset, uint32_t file_size, uint16_t flags);

void     mem_write_page(mem_space space_handle, uintptr_t virt_addr, const void *data, size_t size);

//
// mem_map_portal - creates a "portal" from the current address space into the other
//
// @src_virt_address in the current address space and
// @dest_virt_address in @dest_space will point to the same page. This
// page is allocated if not present.  Unmap the page in the current
// space using mem_unmap_portal when done. It's a good idea to map
// an address covered by an ALLOC area so the page is free'd when the
// process exits.
// A portal overrides the access permissions locally.
//
int mem_map_portal(uintptr_t virt_address,
                   size_t length,
                   mem_space dest_space,
                   uintptr_t dest_virt_address,
                   uint16_t flags);
void mem_unmap_portal(uintptr_t virt_address, size_t length);

p2::opt<uint16_t> mem_area_flags(mem_space space, const void *address);
p2::opt<mem_area> mem_find_area(mem_space space_handle, uintptr_t address);

#endif // !PEOS2_MEMORY_H

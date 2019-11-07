// -*- c++ -*-
//
// Responsible for paging, page allocation, etc.
//

#ifndef PEOS2_MEMORY_H
#define PEOS2_MEMORY_H

#include <stdint.h>
#include <stddef.h>

#define ALIGN_UP(val, align) (((val) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(val, align) ((val) & ~((align) - 1))

#define MEM_PE_P   0x0001  // Present
#define MEM_PE_RW  0x0002  // Read/write
#define MEM_PE_U   0x0004  // User/supervisor (0 = user not allowed)
#define MEM_PE_W   0x0008  // Write through
#define MEM_PE_D   0x0010  // Cache disabled
#define MEM_PE_A   0x0020  // Accessed

#define MEM_PDE_S  0x0080  // Page size (0 = 4kb)

#define MEM_PTE_D  0x0040  // Dirty
#define MEM_PTE_G  0x0080  // Global

struct region {
  uintptr_t start, end;
};

typedef uint16_t mem_adrspc;

void       mem_init(const region *phys_regions, size_t count);

void      *mem_alloc_page();
void      *mem_alloc_page_zero();
void       mem_free_page(void *page);

mem_adrspc mem_create_address_space();
void       mem_map_kernel(mem_adrspc adrspc, uint32_t flags);
void       mem_destroy_address_space(mem_adrspc address_space);
void       mem_activate_address_space(mem_adrspc address_space);
void       mem_map_page(mem_adrspc, uint32_t virt, uint32_t phys, uint16_t flags);


#endif // !PEOS2_MEMORY_H

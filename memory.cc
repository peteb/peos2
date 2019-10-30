#include "memory.h"
#include "assert.h"
#include "screen.h"
#include "x86.h"
#include "protected_mode.h"

#include "support/page_alloc.h"

struct page_dir_entry {
  uint16_t flags:12;
  uint32_t table_11_31:20;
} __attribute__((packed));

struct page_table_entry {
  uint16_t flags:12;
  uint32_t frame_11_31:20;
} __attribute__((packed));

extern "C" void isr_page_fault(isr_registers);

static p2::page_allocator *pmem;

void mem_map_page(void *page_directory, uint32_t virt, uint32_t phys) {
  assert((virt & 0xFFF) == 0 && "can only map on page boundaries");
  assert((phys & 0xFFF) == 0 && "can only map on page boundaries");

  page_dir_entry *page_dir = (page_dir_entry *)page_directory;
  page_table_entry *page_table = nullptr;
  int directory_idx = virt >> 22;

  if (!(page_dir[directory_idx].flags & MEM_PDE_P)) {
    page_table = (page_table_entry *)mem_alloc_page_zero();
    page_dir[directory_idx].table_11_31 = ((uintptr_t)page_table) >> 12;
    page_dir[directory_idx].flags = MEM_PDE_P|MEM_PDE_R|MEM_PDE_U;
  }
  else {
    page_table = (page_table_entry *)(page_dir[directory_idx].table_11_31 << 12);
  }

  int table_idx = (virt >> 12) & 0x3FF;
  page_table[table_idx].frame_11_31 = (phys >> 12);
  page_table[table_idx].flags = MEM_PTE_P|MEM_PTE_R|MEM_PTE_U;
}

void mem_init(const region *regions, size_t region_count) {
  uintptr_t largest_region_size = 0;
  region largest_region = {};

  for (size_t i = 0; i < region_count; ++i) {
    size_t size = regions[i].end - regions[i].start;
    if (size > largest_region_size) {
      largest_region_size = size;
      largest_region = regions[i];
    }
  }

  assert(largest_region_size != 0);

  puts(p2::format<64>("mem: using region %x-%x (%d MB) for page alloc")
       % largest_region.start
       % largest_region.end
       % ((uintptr_t)(largest_region.end - largest_region.start) / 1024 / 1024));

  static p2::page_allocator alloc{largest_region.start, largest_region.end};
  // TODO: fix this hackery
  pmem = &alloc;
}

void mem_init_paging() {
  int_register(INT_PAGEFAULT, isr_page_fault, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P);

  page_dir_entry *page_directory = (page_dir_entry *)mem_create_page_dir();

  // Identity map 5 mb
  for (uint32_t address = 0; address < 5 * 1024 * 1024; address += 0x1000) {
    mem_map_page(page_directory, address, address);
  }

  // Point to our page directory
  asm volatile("mov cr3, %0" : : "a"(page_directory) : "memory");

  // Enable paging
  asm volatile("mov eax, cr0\n"
               "or eax, %0\n"
               "mov cr0, eax"
               :: "i"(CR0_PG));
}

void mem_free_page_dir(void *page_directory) {
  page_dir_entry *page_dir = (page_dir_entry *)page_directory;

  for (int i = 0; i < 1024; ++i) {
    if (page_dir[i].flags & MEM_PDE_P) {
      mem_free_page((void *)(page_dir[i].table_11_31 << 12));
    }
  }

  mem_free_page(page_dir);
}

void *mem_create_page_dir() {
  return mem_alloc_page_zero();
}

void *mem_alloc_page() {
  assert(pmem);
  void *mem = pmem->alloc_page();
  puts(p2::format<64>("mem: allocated 4k page at %x, space left: %d")
       % (uintptr_t)mem
       % pmem->free_space());
  return mem;
}

void *mem_alloc_page_zero() {
  assert(pmem);
  void *mem = pmem->alloc_page_zero();
  puts(p2::format<64>("mem: allocated 4k page at %x, space left: %d")
       % (uintptr_t)mem
       % pmem->free_space());
  return mem;
}

void mem_free_page(void *page) {
  assert(pmem);
  pmem->free_page(page);
  puts(p2::format<64>("mem: freed 4k page at %x, space left: %d")
       % (uintptr_t)page
       % pmem->free_space());
}


extern "C" void int_page_fault(isr_registers regs) {
  p2::string<256> buf;
  regs.to_string(buf);
  puts(buf);
  panic("Page fault");
}

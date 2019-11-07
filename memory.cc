#include "memory.h"
#include "assert.h"
#include "screen.h"
#include "x86.h"
#include "protected_mode.h"
#include "memareas.h"
#include "multiboot.h"
#include "debug.h"

#include "support/page_alloc.h"
#include "support/pool.h"

// Structs
struct page_dir_entry {
  uint16_t flags:12;
  uint32_t table_11_31:20;
} __attribute__((packed));

struct page_table_entry {
  uint16_t flags:12;
  uint32_t frame_11_31:20;
} __attribute__((packed));

struct space_info {
  page_dir_entry *page_dir;
};

// Externs
extern "C" void isr_page_fault(isr_registers);
extern int kernel_end;
extern char __start_READONLY, __stop_READONLY;

// Global state
static char page_dir_area[sizeof(page_dir_entry) * 1024 * 100] alignas(0x1000);
static char page_table_area[sizeof(page_table_entry) * 1024 * 100] alignas(0x1000);

static p2::page_allocator page_dir_allocator((uintptr_t)page_dir_area,
                                             (uintptr_t)(page_dir_area + sizeof(page_dir_area)));
static p2::page_allocator page_table_allocator((uintptr_t)page_table_area,
                                               (uintptr_t)(page_table_area + sizeof(page_table_area)));

static p2::page_allocator *pmem;

static p2::pool<space_info, 16, mem_space> spaces;
static mem_space current_space = spaces.end();


void mem_init(const region *phys_region)
{
  puts(p2::format<64>("mem: using region %x-%x (%d MB) for page alloc",
                      phys_region->start,
                      phys_region->end,
                      ((uintptr_t)(phys_region->end - phys_region->start) / 1024 / 1024)));

  static p2::page_allocator alloc{phys_region->start, phys_region->end};
  // TODO: fix this hackery
  pmem = &alloc;

  int_register(INT_PAGEFAULT, isr_page_fault, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P);
}

void mem_free_page_dir(void *page_directory)
{
  page_dir_entry *page_dir = (page_dir_entry *)page_directory;

  for (int i = 0; i < 1024; ++i) {
    if (page_dir[i].flags & MEM_PE_P) {
      mem_free_page((void *)(page_dir[i].table_11_31 << 12));
    }
  }

  mem_free_page(page_dir);
}

void *mem_create_page_dir()
{
  return mem_alloc_page_zero();
}

void *mem_alloc_page()
{
  assert(pmem);
  void *mem = pmem->alloc_page();
  dbg_puts(mem, "allocated 4k page at %x, space left: %d", (uintptr_t)mem, pmem->free_space());
  return mem;
}

void *mem_alloc_page_zero()
{
  assert(pmem);
  void *mem = pmem->alloc_page_zero();
  dbg_puts(mem, "allocated 4k page at %x, space left: %d", (uintptr_t)mem, pmem->free_space());
  return mem;
}

void mem_free_page(void *page)
{
  assert(pmem);
  pmem->free_page(page);
  dbg_puts(mem, "freed 4k page at %x, space left: %d", (uintptr_t)page, pmem->free_space());
}

mem_space mem_create_space()
{
  page_dir_entry *page_dir = (page_dir_entry *)page_dir_allocator.alloc_page_zero();
  dbg_puts(mem, "created page dir %x", (uintptr_t)page_dir);
  return spaces.push_back({page_dir});
}

void mem_map_kernel(mem_space space_handle, uint32_t flags)
{
  // TODO: we don't really want to know about multiboot everywhere
  uintptr_t end = multiboot_last_address();
  end = p2::max(end, (uintptr_t)&kernel_end);

  for (uintptr_t address = KERNEL_VIRTUAL_BASE;
       address < ALIGN_UP(end, 0x1000);
       address += 0x1000) {

    if (address >= (uintptr_t)&__start_READONLY && address <= ALIGN_UP((uintptr_t)&__stop_READONLY, 0x1000))
      mem_map_page(space_handle, address, KERNVIRT2PHYS(address), flags & ~MEM_PE_RW);
    else
      mem_map_page(space_handle, address, KERNVIRT2PHYS(address), flags);
  }
}

void mem_destroy_space(mem_space space_handle)
{
  if (space_handle == current_space) {
    // We assume that the kernel fallback address space (0) is
    // compatible with `adrspc`.

    // TODO: remove magic 0 constant
    mem_activate_space(0);
  }

  space_info *space = &spaces[space_handle];

  for (int i = 0; i < 1024; ++i) {
    if (!(space->page_dir[i].flags & MEM_PE_P)) {
      continue;
    }

    dbg_puts(mem, "deleting page table %x", PHYS2KERNVIRT(space->page_dir[i].table_11_31 << 12));
    page_table_allocator.free_page((void *)PHYS2KERNVIRT(space->page_dir[i].table_11_31 << 12));
    space->page_dir[i].table_11_31 = 0;
    space->page_dir[i].flags = 0;
  }

  dbg_puts(mem, "deleting page dir %x", (uintptr_t)space->page_dir);
  page_dir_allocator.free_page(space->page_dir);
  space->page_dir = nullptr;
  spaces.erase(space_handle);
}

void mem_activate_space(mem_space space_handle)
{
  if (current_space == space_handle) {
    return;
  }

  current_space = space_handle;
  space_info *space = &spaces[space_handle];
  uintptr_t page_dir_phys = KERNVIRT2PHYS((uintptr_t)space->page_dir);

  asm volatile("mov cr3, %0" : : "a"(page_dir_phys) : "memory");
  // TODO: fix all inline asm to have the same syntax (AT&T vs Intel) as *.s files
}

void mem_map_page(mem_space space_handle, uint32_t virt, uint32_t phys, uint16_t flags)
{
  assert((virt & 0xFFF) == 0 && "can only map on page boundaries");
  assert((phys & 0xFFF) == 0 && "can only map on page boundaries");

  page_dir_entry *page_dir = spaces[space_handle].page_dir;
  page_table_entry *page_table = nullptr;
  int directory_idx = virt >> 22;
  uint16_t pde_flags = flags & ~(MEM_PDE_S|MEM_PTE_D);

  if (!(page_dir[directory_idx].flags & MEM_PE_P)) {
    page_table = (page_table_entry *)page_table_allocator.alloc_page_zero();
    page_dir[directory_idx].table_11_31 = KERNVIRT2PHYS((uintptr_t)page_table) >> 12;
    page_dir[directory_idx].flags = pde_flags;
    dbg_puts(mem, "created page table %x", (uintptr_t)page_table);
  }
  else {
    //assert(page_dir[directory_idx].flags == pde_flags && "conflicting flags for pde");
    page_dir[directory_idx].flags |= pde_flags;
    page_table = (page_table_entry *)PHYS2KERNVIRT(page_dir[directory_idx].table_11_31 << 12);
  }

  int table_idx = (virt >> 12) & 0x3FF;
  page_table[table_idx].frame_11_31 = (phys >> 12);
  page_table[table_idx].flags = flags;

  // TODO: flush tlb if current address space
}

mem_area mem_map_linear(mem_space space_handle, uintptr_t start, uintptr_t end, uintptr_t phys_start, uint16_t flags)
{
  (void)space_handle;
  (void)start;
  (void)end;
  (void)phys_start;
  (void)flags;
  return 0;
}

extern "C" void int_page_fault(isr_registers regs)
{
  p2::string<256> buf;
  regs.to_string(buf);
  puts(buf);

  uint32_t cr2 = 0;
  asm volatile("mov cr2, eax" : "=a"(cr2));

  panic(p2::format<64>("Page fault (error %x) at %x", regs.error_code, cr2).str().c_str());
}

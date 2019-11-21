#include "memory.h"
#include "screen.h"
#include "x86.h"
#include "protected_mode.h"
#include "memareas.h"
#include "multiboot.h"
#include "debug.h"
#include "process.h"
#include "syscalls.h"
#include "syscall_utils.h"
#include "filesystem.h"
#include "memory_private.h"

#include "support/page_alloc.h"
#include "support/pool.h"
#include "support/optional.h"
#include "support/assert.h"

// Externs
extern "C" void isr_page_fault(isr_registers *);
extern int kernel_end;
extern char __start_READONLY, __stop_READONLY;

// Forward decls
static void unmap_area(mem_space space_handle, mem_area area_handle);
static void *alloc_page();
static void free_page(void *page);
static void map_page(mem_space space, uint32_t virt, uint32_t phys, uint16_t flags);
static int syscall_mmap(void *start, void *end, int fd, uint32_t offset, uint8_t flags);
static p2::opt<mem_area> find_area(mem_space space_handle, uintptr_t address);
static page_table_entry *find_pte(const space_info &space, uintptr_t virtual_address);
static uintptr_t pte_frame(const page_table_entry *pte);
static uint16_t page_flags(uint16_t mem_flags);

// Global state
static p2::internal_page_allocator<sizeof(page_dir_entry)   * 1024 * 100, 0x1000> page_dir_allocator;
static p2::internal_page_allocator<sizeof(page_table_entry) * 1024 * 100, 0x1000> page_table_allocator;

static p2::page_allocator *user_space_allocator;

static p2::pool<space_info, 128, mem_space> spaces;
static mem_space current_space = spaces.end();
static mem_space start_space;


void mem_init(const region *phys_region)
{
  puts(p2::format<64>("mem: using region %x-%x (%d MB) for page alloc",
                      phys_region->start,
                      phys_region->end,
                      ((uintptr_t)(phys_region->end - phys_region->start) / 1024 / 1024)));

  // TODO: fix this hackery
  static p2::page_allocator alloc{{phys_region->start, phys_region->end}, KERNEL_VIRTUAL_BASE};
  user_space_allocator = &alloc;

  start_space = mem_create_space();
  mem_map_kernel(start_space, MEM_AREA_READWRITE);
  mem_activate_space(start_space);

  // Interrupts
  int_register(INT_PAGEFAULT, isr_page_fault, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P);

  // Syscalls
  syscall_register(SYSCALL_NUM_MMAP, (syscall_fun)syscall_mmap);
}

mem_space mem_create_space()
{
  page_dir_entry *page_dir = (page_dir_entry *)page_dir_allocator.alloc_page_zero();
  mem_space space_handle = spaces.emplace_back(page_dir);
  dbg_puts(mem, "created space %d with page dir %x", space_handle, (uintptr_t)page_dir);
  return space_handle;
}


void mem_destroy_space(mem_space space_handle)
{
  assert(space_handle != current_space && "cannot destroy current space");
  space_info *space = &spaces[space_handle];

  for (size_t i = 0; i < space->areas.watermark(); ++i) {
    if (!space->areas.valid(i))
      continue;

    // This will remove from the area list and mark the PTE as
    // non-present, this isn't strictly necessary
    unmap_area(space_handle, i);
  }

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

//
// mem_unmap_not_matching - unmaps all areas where flags aren't
// matching. Be careful not to unmap areas that you need.
//
void mem_unmap_not_matching(mem_space space_handle, uint16_t flags)
{
  space_info &space = spaces[space_handle];

  for (size_t i = 0; i < space.areas.watermark(); ++i) {
    if (!space.areas.valid(i))
      continue;

    if (!(space.areas[i].flags & flags)) {
      dbg_puts(mem, "unmapping area %d", i);
      unmap_area(space_handle, i);
    }
  }

  mem_print_space(space_handle);
}

void mem_print_space(mem_space space_handle)
{
  space_info &space = spaces[space_handle];

  for (int i = 0; i < space.areas.watermark(); ++i) {
    if (!space.areas.valid(i))
      continue;

    area_info &area = space.areas[i];
    dbg_puts(mem, "%d: area %x-%x (type %d flags %x)", i, area.start, area.end, area.type, area.flags);
  }
}

static void copy_page_contents(uintptr_t source_phys, uintptr_t dest_phys)
{
  assert(!(source_phys & 0xFFF));
  assert(!(dest_phys & 0xFFF));
  mem_space current_space = proc_get_space(*proc_current_pid());

  const uintptr_t source_virt = 0xFFFE0000;
  const uintptr_t dest_virt =   0xFFFE1000;

  // Map both pages in current space temporarily so we can copy
  map_page(current_space, source_virt, source_phys, MEM_PE_P|MEM_PE_RW|MEM_PE_D);
  map_page(current_space, dest_virt, dest_phys, MEM_PE_P|MEM_PE_RW|MEM_PE_D);

  memcpy((void *)dest_virt, (void *)source_virt, 0x1000);

  // Remove the mapping
  map_page(current_space, source_virt, 0, 0);
  map_page(current_space, dest_virt, 0, 0);
}

static void copy_area(area_info &source_area, space_info &source_space, mem_space dest_space_handle)
{
  // TODO: areas are contenders for polymorphism -- we've got a couple
  // of places where we execute logic depending on the type
  if (source_area.type == AREA_LINEAR_MAP) {
    dbg_puts(mem, "copy linear");

    linear_map_info &source_map_info = source_space.linear_maps[source_area.info_handle];
    mem_map_linear(dest_space_handle,
                   source_area.start,
                   source_area.end,
                   source_map_info.phys_start,
                   source_area.flags);

    // Copy existing mappings, needed to get kernel stuff like ISRs, otherwise we triple fault
    for (uintptr_t page_address = source_area.start;
         page_address < source_area.end;
         page_address += 0x1000) {

      if (auto *pte = find_pte(source_space, page_address); pte && pte->flags & MEM_PE_P) {
        map_page(dest_space_handle, page_address, pte_frame(pte), pte->flags);
      }
    }
  }
  else if (source_area.type == AREA_FILE) {
    dbg_puts(mem, "copy file");
    file_map_info &source_map_info = source_space.file_maps[source_area.info_handle];

    mem_map_fd(dest_space_handle,
               source_area.start,
               source_area.end,
               source_map_info.fd,
               source_map_info.offset,
               source_map_info.size,
               source_area.flags);
  }
  else if (source_area.type == AREA_ALLOC) {
    // Copy all present pages
    dbg_puts(mem, "copy alloc");

    mem_map_alloc(dest_space_handle,
                  source_area.start,
                  source_area.end,
                  source_area.flags);

    // TODO: change this brute force method into something smarter
    // that reads the paging structure
    for (uintptr_t page_address = source_area.start;
         page_address < source_area.end;
         page_address += 0x1000) {

      if (auto *pte = find_pte(source_space, page_address); pte && pte->flags & MEM_PE_P) {
        dbg_puts(mem, "copying page %x", page_address);

        uintptr_t phys_address = (uintptr_t)alloc_page();
        map_page(dest_space_handle, page_address, phys_address, page_flags(source_area.flags));
        copy_page_contents(pte_frame(pte), phys_address);
      }
    }
  }
}

p2::res<mem_space> mem_fork_space(mem_space space_handle)
{
  space_info &source_space = spaces[space_handle];
  mem_space new_space_handle = mem_create_space();

  dbg_puts(mem, "forking space %d", space_handle);

  // Copy all linear and file maps directly
  // ALLOC needs to be copied page by page, immediately
  // so we need to map both source and dest pages in current space
  for (int i = 0; i < source_space.areas.watermark(); ++i) {
    if (!source_space.areas.valid(i))
      continue;

    area_info &source_area = source_space.areas[i];

    if (source_area.flags & MEM_AREA_NO_FORK)
      continue;

    copy_area(source_area, source_space, new_space_handle);
  }

  mem_print_space(new_space_handle);
  return p2::success(new_space_handle);
}

void mem_activate_space(mem_space space_handle)
{
  // TODO: get rid of this function! it shouldn't be used now when we
  // don't have all kernel stacks mapped in all spaces

  current_space = space_handle;
  space_info *space = &spaces[space_handle];
  uintptr_t page_dir_phys = KERNVIRT2PHYS((uintptr_t)space->page_dir);
  asm volatile("mov cr3, %0" :: "a"(page_dir_phys) : "memory");
  // TODO: fix all inline asm to have the same syntax (AT&T vs Intel) as *.s files
}

void mem_set_current_space(mem_space space_handle)
{
  current_space = space_handle;
}

uintptr_t mem_page_dir(mem_space space_handle)
{
  return KERNVIRT2PHYS((uintptr_t)spaces[space_handle].page_dir);
}

static uint16_t page_flags(uint16_t mem_flags)
{
  uint16_t converted = MEM_PE_P;

  if (mem_flags & MEM_AREA_READWRITE)
    converted |= MEM_PE_RW;

  if (mem_flags & MEM_AREA_USER)
    converted |= MEM_PE_U;

  if (mem_flags & MEM_AREA_CACHE_DISABLED)
    converted |= MEM_PE_D;

  if (mem_flags & MEM_AREA_GLOBAL)
    converted |= MEM_PTE_G;

  return converted;
}

static page_table_entry *find_pte(const space_info &space, uintptr_t virtual_address)
{
  int pde_idx = virtual_address >> 22;
  const page_dir_entry &pde = space.page_dir[pde_idx];
  if (!(pde.flags & MEM_PE_P))
    return nullptr;

  page_table_entry *page_table = (page_table_entry *)PHYS2KERNVIRT(pde.table_11_31 << 12);
  assert(page_table);

  page_table_entry *pte = &page_table[(virtual_address >> 12) & 0x3FF];
  if (!(pte->flags & MEM_PE_P))
    return nullptr;

  return pte;
}

static uintptr_t pte_frame(const page_table_entry *pte)
{
  return pte->frame_11_31 << 12;
}

//
// mem_write_page - copies data from the kernel into the space
//
void mem_write_page(mem_space space_handle, uintptr_t virt_addr, const void *data, size_t size)
{
  assert(!(virt_addr & 0xFFF));

  // We can only write to ALLOC areas for now
  auto area_handle = find_area(space_handle, (uintptr_t)virt_addr);
  assert(area_handle && "address to map must be in a valid area");

  space_info &dest_space = spaces[space_handle];
  area_info &dest_area = dest_space.areas[*area_handle];
  assert(dest_area.type == AREA_ALLOC);

  auto *pte = find_pte(dest_space, virt_addr);

  if (!pte || !(pte->flags & MEM_PE_P)) {
    // Page isn't mapped yet, so do that
    map_page(space_handle, virt_addr, (uintptr_t)alloc_page(), page_flags(dest_area.flags));
    pte = find_pte(dest_space, virt_addr);
    assert(pte && (pte->flags & MEM_PE_P));
  }

  dbg_puts(mem, "writing to virt address %x (%x) from %x (%d bytes)",
           (uintptr_t)virt_addr,
           (uintptr_t)pte_frame(pte),
           (uintptr_t)data,
           size);

  assert(size <= 0x1000);
  const uintptr_t mapped_address = KERNEL_SCRATCH_BASE;
  map_page(current_space, mapped_address, pte_frame(pte), MEM_PE_P|MEM_PE_RW|MEM_PE_W|MEM_PE_D);
  memcpy((void *)mapped_address, data, size);
  map_page(current_space, mapped_address, 0, 0);
}

int mem_map_portal(uintptr_t virt_address, size_t length, mem_space dest_space, uintptr_t dest_virt_address, uint16_t flags)
{
  space_info &dest_space_ = spaces[dest_space];

  auto start_area = find_area(dest_space, dest_virt_address);
  auto end_area = find_area(dest_space, dest_virt_address + length - 1);

  assert(start_area && "starting address must be covered by an area in dest space to create a portal");
  assert(end_area && "ending address must be covered by an area in dest space to create a portal");
  assert(dest_space_.areas[*start_area].type == AREA_ALLOC && "area must be an alloc area");
  assert(*start_area == *end_area && "start and end area must be the same");

  area_info &dest_area = dest_space_.areas[*start_area];

  for (uintptr_t offset = 0; offset < length; offset += 0x1000) {
    if (auto *pte = find_pte(dest_space_, dest_virt_address + offset); pte && pte->flags & MEM_PE_P) {
      // Page already exists, so let's point to its address
      map_page(current_space, virt_address + offset, pte_frame(pte), page_flags(flags));
    }
    else {
      // Page doesn't exist. Map it in source using the area's flags
      // and in the calling process using @flags

      // TODO: maybe we should call the page fault handler instead of
      // just assuming it's an ALLOC area?
      uintptr_t phys_address = (uintptr_t)alloc_page();
      map_page(current_space, virt_address + offset, phys_address, page_flags(flags));
      map_page(dest_space, dest_virt_address + offset, phys_address, page_flags(dest_area.flags));
    }
  }

  return 0;
}

void mem_unmap_portal(uintptr_t virt_address, size_t length)
{
  for (uintptr_t offset = 0; offset < length; offset += 0x1000) {
    map_page(current_space, virt_address + offset, 0xDEAD0000, 0);
  }
}

static inline void invlpg(uintptr_t addr)
{
  asm volatile("invlpg [%0]" :: "a"(addr) : "memory");
}

void map_page(mem_space space_handle, uint32_t virt, uint32_t phys, uint16_t flags)
{
  assert((virt & 0xFFF) == 0 && "can only map on page boundaries");
  assert((phys & 0xFFF) == 0 && "can only map on page boundaries");

  page_dir_entry *page_dir = spaces[space_handle].page_dir;
  page_table_entry *page_table = nullptr;
  int directory_idx = virt >> 22;
  uint16_t pde_flags = flags & ~(MEM_PDE_S|MEM_PTE_D);

  if (!(page_dir[directory_idx].flags & MEM_PE_P)) {
    // Don't create a page dir if we want to unmap the page
    if (!(flags & MEM_PE_P))
      return;

    page_table = (page_table_entry *)page_table_allocator.alloc_page_zero();
    page_dir[directory_idx].table_11_31 = KERNVIRT2PHYS((uintptr_t)page_table) >> 12;
    page_dir[directory_idx].flags = pde_flags;
  }
  else {
    //assert(page_dir[directory_idx].flags == pde_flags && "conflicting flags for pde");
    page_dir[directory_idx].flags |= pde_flags;
    page_table = (page_table_entry *)PHYS2KERNVIRT(page_dir[directory_idx].table_11_31 << 12);
  }

  int table_idx = (virt >> 12) & 0x3FF;
  page_table[table_idx].frame_11_31 = (phys >> 12);
  page_table[table_idx].flags = flags;

  if (space_handle == current_space) {
    invlpg(virt);
  }
}

void mem_map_kernel(mem_space space_handle, uint16_t flags)
{
  // TODO: we don't really want to know about multiboot everywhere
  uintptr_t end = p2::max(multiboot_last_address(), (uintptr_t)&kernel_end);
  assert(user_space_allocator);

  struct {
    uintptr_t virt_address;
    int flags;
  } segments[] = {
    {0,                                                                    -1},
    {KERNEL_VIRTUAL_BASE,                                                  flags & ~MEM_AREA_EXECUTABLE},
    // Misc stuff before the code starts

    {ALIGN_UP((uintptr_t)&__start_READONLY, 0x1000),                       flags & ~MEM_AREA_READWRITE},
    // Kernel .text, .rodata

    {ALIGN_DOWN((uintptr_t)&__stop_READONLY, 0x1000),                      flags & ~MEM_AREA_EXECUTABLE},
    // Kernel .bss, .data, etc

    {ALIGN_UP(end, 0x1000),                                                -1},
    // ??

    {PHYS2KERNVIRT(user_space_allocator->bookkeeping_phys_region().start), flags & ~MEM_AREA_EXECUTABLE},
    // Metadata for the physical page allocator

    {PHYS2KERNVIRT(user_space_allocator->bookkeeping_phys_region().end),   -1},
  };

  uintptr_t current_address = 0;
  int last_flags = 0;

  for (size_t i = 0; i < ARRAY_SIZE(segments); ++i) {
    if (segments[i].virt_address != current_address && last_flags != -1) {
      // Map previous segment
      mem_map_linear_eager(space_handle,
                           current_address,
                           segments[i].virt_address,
                           KERNVIRT2PHYS(current_address),
                           last_flags);
      dbg_puts(mem, "%d: map %x-%x @ %x: %x", i, current_address, segments[i].virt_address, KERNVIRT2PHYS(current_address), last_flags);
      assert(segments[i].virt_address >= current_address);
    }

    current_address = segments[i].virt_address;
    last_flags = segments[i].flags;
  }
}

static bool overlaps_existing_area(mem_space space_handle, uintptr_t start, uintptr_t end)
{
  space_info &space = spaces[space_handle];

  for (size_t i = 0; i < space.areas.watermark(); ++i) {
    if (!space.areas.valid(i))
      continue;

    area_info &area = space.areas[i];

    if (start < area.end && end > area.start)
      return true;
  }

  return false;
}

mem_area mem_map_linear(mem_space space_handle,
                        uintptr_t start,
                        uintptr_t end,
                        uintptr_t phys_start,
                        uint16_t flags)
{
  assert(!overlaps_existing_area(space_handle, start, end));
  space_info *space = &spaces[space_handle];
  uint16_t map_handle = space->linear_maps.push_back({phys_start});
  return space->areas.push_back({start, end, AREA_LINEAR_MAP, flags, map_handle});
}

mem_area mem_map_linear_eager(mem_space space_handle,
                              uintptr_t start,
                              uintptr_t end,
                              uintptr_t phys_start,
                              uint16_t flags)
{
  assert(!overlaps_existing_area(space_handle, start, end));

  uintptr_t phys_address = phys_start;

  for (uintptr_t virt_address = start;
       virt_address < end;
       virt_address += 0x1000) {
    map_page(space_handle, virt_address, phys_address, page_flags(flags));
    phys_address += 0x1000;
  }

  space_info *space = &spaces[space_handle];
  uint16_t map_handle = space->linear_maps.push_back({phys_start});
  return space->areas.push_back({start, end, AREA_LINEAR_MAP, flags, map_handle});
}

mem_area mem_map_alloc(mem_space space_handle, uintptr_t start, uintptr_t end, uint16_t flags)
{
  dbg_puts(mem, "mapping alloc in %d: %x - %x", space_handle, start, end);
  assert(!overlaps_existing_area(space_handle, start, end));

  // Another nifty way of doing allocations would be to get rid of
  // AREA_ALLOC and instead let people mmap /dev/zero, decreasing
  // complexity in the kernel. On the other hand, I see a number of
  // cons; 1) every process that needs more memory would need to keep
  // an fd open to /dev/zero, requiring slightly more memory. 2) the
  // extra fd risks user space mistakes causing "chaos", for example
  // if they were to replace that fd with a file or stdin. 3) Not
  // separating allocation from file mapping can lead to a slightly
  // higher risk of a mistake in the kernel leading to user space
  // pages getting unmapped.  Using AREA_ALLOC doesn't require any
  // extra space over an mmap -- it's actually more memory efficient.
  return spaces[space_handle].areas.push_back({start, end, AREA_ALLOC, flags, 0});
}

mem_area mem_map_fd(mem_space space_handle,
                    uintptr_t start,
                    uintptr_t end,
                    int fd,
                    uint32_t offset,
                    uint32_t file_size,
                    uint16_t flags)
{
  assert(!overlaps_existing_area(space_handle, start, end));
  assert(fd > 2 && "fd cannot be one of the standard fds");

  space_info &space = spaces[space_handle];
  // TODO: check reference
  uint16_t map_handle = space.file_maps.push_back({fd, offset, file_size});
  return space.areas.push_back({start, end, AREA_FILE, flags, map_handle});
}

static p2::opt<mem_area> find_area(mem_space space_handle, uintptr_t address)
{
  space_info &space = spaces[space_handle];

  for (int i = 0; i < space.areas.watermark(); ++i) {
    if (!space.areas.valid(i))
      continue;

    area_info &area = space.areas[i];
    if (address >= area.start && address < area.end)
      return i;
  }

  return {};
}

p2::opt<uint16_t> mem_area_flags(mem_space space_handle, const void *address)
{
  if (auto area_handle = find_area(space_handle, (uintptr_t)address))
    return spaces[space_handle].areas[*area_handle].flags;
  else
    return {};
}

typedef void (*page_fault_handler)(area_info &area, uintptr_t faulted_address);

static void page_fault_linear_map(area_info &area, uintptr_t faulted_address)
{
  linear_map_info &lm_info = spaces[current_space].linear_maps[area.info_handle];
  uintptr_t page_address = ALIGN_DOWN(faulted_address, 0x1000);
  ptrdiff_t area_offset = page_address - area.start;
  dbg_puts(mem, "linear map; mapping %x to %x", page_address, lm_info.phys_start + area_offset);
  map_page(current_space, page_address, lm_info.phys_start + area_offset, page_flags(area.flags));
}

static void page_fault_alloc(area_info &area, uintptr_t faulted_address)
{
  uintptr_t page_address = ALIGN_DOWN(faulted_address, 0x1000);
  uintptr_t phys_block = (uintptr_t)alloc_page();
  dbg_puts(mem, "alloc map; allocated %x and mapping it at %x", phys_block, page_address);

  uint16_t writable = area.flags & MEM_AREA_READWRITE;
  // Temporarily set the page to readwrite so we can null the page
  map_page(current_space, page_address, phys_block, page_flags(area.flags | MEM_AREA_READWRITE));
  memset((void *)page_address, 0, 0x1000);

  // Then turn it back to readonly
  if (!writable)
    map_page(current_space, page_address, phys_block, page_flags(area.flags & ~MEM_AREA_READWRITE));
}

static void page_fault_file(area_info &area, uintptr_t faulted_address)
{
  uintptr_t page_address = ALIGN_DOWN(faulted_address, 0x1000);
  uintptr_t phys_block = (uintptr_t)alloc_page();
  uint16_t writable = area.flags & MEM_AREA_READWRITE;
  map_page(current_space, page_address, phys_block, page_flags(area.flags | MEM_AREA_READWRITE));
  memset((void *)page_address, 0, 0x1000);

  file_map_info &fm_info = spaces[current_space].file_maps[area.info_handle];
  ptrdiff_t area_offset = page_address - area.start;
  uint32_t file_offset = area_offset + fm_info.offset;

  dbg_puts(mem, "file map; allocated %x and mapping it at %x. fd is %d", phys_block, page_address, fm_info.fd);


  // TODO: syscall here is unnecessary; we're already in the kernel
  if (vfs_seek(proc_get_file_context(*proc_current_pid()), fm_info.fd, file_offset, SEEK_BEG) < 0) {
    panic("failed to seek");
  }

  uint32_t read_count = p2::min<uint32_t>(fm_info.offset + fm_info.size - file_offset, 0x1000);
  dbg_puts(mem, "Reading max %d bytes", read_count);

  if (!vfs_read(proc_get_file_context(*proc_current_pid()), fm_info.fd, (char *)page_address, read_count)) {
    panic("failed to read");
  }

  // Turn the page back to readonly if needed
  if (!writable)
    map_page(current_space, page_address, phys_block, page_flags(area.flags & ~MEM_AREA_READWRITE));
}

static void unmap_area(mem_space space_handle, mem_area area_handle)
{
  auto &space = spaces[space_handle];
  auto &area = space.areas[area_handle];

  for (uintptr_t page_address = area.start;
       page_address < area.end;
       page_address += 0x1000) {

    if (auto *pte = find_pte(space, page_address); pte && pte->flags & MEM_PE_P) {
      if (area.type == AREA_ALLOC || area.type == AREA_FILE) {
        free_page((void *)pte_frame(pte));
      }

      map_page(space_handle, page_address, 0xDEAD000, 0);
    }
  }

  space.areas.erase(area_handle);
}

extern "C" void int_page_fault(isr_registers *regs)
{
  uint32_t faulted_address = 0;
  asm volatile("mov eax, cr2" : "=a"(faulted_address));

  if (regs->error_code & 1) {
    const char *access_type = "read";
    const char *cpl = "supervisor";

    if (regs->error_code & 0x2)
      access_type = "write";

    if (regs->error_code & 0x3)
      cpl = "user";

    dbg_puts(mem, "%s process tried to %s protected page at %x", cpl, access_type, faulted_address);
    kill_caller();
    return;
  }

  p2::optional<mem_area> area_handle = find_area(current_space, faulted_address);

  if (!area_handle) {
    dbg_puts(mem, "process tried to access un-mapped area at %x in space %d (esp: %x, eip: %x)",
             faulted_address,
             current_space,
             regs->user_esp,
             regs->eip);
    dbg_break();
    kill_caller();
    return;
  }

  dbg_puts(mem, "page fault (error %x) at %x, area %d", regs->error_code, faulted_address, *area_handle);

  area_info &area = spaces[current_space].areas[*area_handle];
  page_fault_handler handler = nullptr;

  switch (area.type) {
  case AREA_LINEAR_MAP:
    handler = page_fault_linear_map;
    break;

  case AREA_ALLOC:
    handler = page_fault_alloc;
    break;

  case AREA_FILE:
    handler = page_fault_file;
    break;

  default:
    panic("invalid area type");
  }

  handler(area, faulted_address);
}

static int syscall_mmap(void *start, void *end, int fd, uint32_t offset, uint8_t flags)
{
  // TODO: check that the pointers are in valid space
  // TODO: add parameter for size!
  mem_map_fd(current_space,
             (uintptr_t)start,
             (uintptr_t)end,
             fd,
             offset,
             p2::numeric_limits<uint32_t>::max(),
             flags | MEM_AREA_USER);
  // TODO: handle errors
  return 0;
}

static void *alloc_page()
{
  assert(user_space_allocator);
  void *mem = user_space_allocator->alloc_page();
  dbg_puts(mem, "allocated 4k page at %x, pages left: %d", (uintptr_t)mem, user_space_allocator->free_pages());
  return mem;
}

static void free_page(void *page)
{
  assert(user_space_allocator);
  user_space_allocator->free_page(page);
  dbg_puts(mem, "freed 4k page at %x, pages left: %d", (uintptr_t)page, user_space_allocator->free_pages());
}

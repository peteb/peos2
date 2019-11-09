#include "memory.h"
#include "assert.h"
#include "screen.h"
#include "x86.h"
#include "protected_mode.h"
#include "memareas.h"
#include "multiboot.h"
#include "debug.h"
#include "process.h"
#include "syscalls.h"

#include "support/page_alloc.h"
#include "support/pool.h"
#include "support/optional.h"

#define AREA_LINEAR_MAP 1
#define AREA_ALLOC      2
#define AREA_FILE       3

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
  uint8_t type, flags;
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
  p2::pool<area_info, 8> areas;
  p2::pool<linear_map_info, 4> linear_maps;
  p2::pool<file_map_info, 4> file_maps;
};


// Externs
extern "C" void isr_page_fault(isr_registers);
extern int kernel_end;
extern char __start_READONLY, __stop_READONLY;

// Forward decls
static void free_alloc_area(space_info &space, area_info &area);
static void free_page(void *page);
static int syscall_mmap(void *start, void *end, int fd, uint32_t offset, uint8_t flags);

// Global state
static p2::internal_page_allocator<sizeof(page_dir_entry)   * 1024 * 100, 0x1000> page_dir_allocator;
static p2::internal_page_allocator<sizeof(page_table_entry) * 1024 * 100, 0x1000> page_table_allocator;

static p2::page_allocator *user_space_allocator;

static p2::pool<space_info, 16, mem_space> spaces;
static mem_space current_space = spaces.end();


void mem_init(const region *phys_region)
{
  puts(p2::format<64>("mem: using region %x-%x (%d MB) for page alloc",
                      phys_region->start,
                      phys_region->end,
                      ((uintptr_t)(phys_region->end - phys_region->start) / 1024 / 1024)));

  // TODO: fix this hackery
  static p2::page_allocator alloc{{phys_region->start, phys_region->end}, KERNEL_VIRTUAL_BASE};
  user_space_allocator = &alloc;

  // Interrupts
  int_register(INT_PAGEFAULT, isr_page_fault, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P);

  // Syscalls
  syscall_register(SYSCALL_NUM_MMAP, (syscall_fun)syscall_mmap);
}

void mem_free_page_dir(void *page_directory)
{
  page_dir_entry *page_dir = (page_dir_entry *)page_directory;

  for (int i = 0; i < 1024; ++i) {
    if (page_dir[i].flags & MEM_PE_P) {
      free_page((void *)(page_dir[i].table_11_31 << 12));
    }
  }

  free_page(page_dir);
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

mem_space mem_create_space()
{
  page_dir_entry *page_dir = (page_dir_entry *)page_dir_allocator.alloc_page_zero();
  dbg_puts(mem, "created page dir %x", (uintptr_t)page_dir);
  return spaces.emplace_back(page_dir);
}

void mem_destroy_space(mem_space space_handle)
{
  if (space_handle == current_space) {
    // We assume that the kernel fallback space (0) is compatible with
    // the current space.

    // TODO: don't use a magic 0 constant
    mem_activate_space(0);
  }

  space_info *space = &spaces[space_handle];

  for (size_t i = 0; i < space->areas.watermark(); ++i) {
    if (!space->areas.valid(i))
      continue;

    if (space->areas[i].type == AREA_ALLOC || space->areas[i].type == AREA_FILE)
      free_alloc_area(*space, space->areas[i]);
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

void mem_activate_space(mem_space space_handle)
{
  current_space = space_handle;
  space_info *space = &spaces[space_handle];
  uintptr_t page_dir_phys = KERNVIRT2PHYS((uintptr_t)space->page_dir);
  asm volatile("mov cr3, %0" : : "a"(page_dir_phys) : "memory");
  // TODO: fix all inline asm to have the same syntax (AT&T vs Intel) as *.s files
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

  if (space_handle == current_space) {
    // Reload CR3 to flush TLB. This is probably not good for performance
    // TODO: use INVLPG instead?
    mem_activate_space(space_handle);
  }
}

void mem_map_kernel(mem_space space_handle, uint32_t flags)
{
  // TODO: we don't really want to know about multiboot everywhere
  uintptr_t end = p2::max(multiboot_last_address(), (uintptr_t)&kernel_end);

  // Map kernel sections (.text, .bss, .rodata, etc)
  for (uintptr_t address = KERNEL_VIRTUAL_BASE;
       address < ALIGN_UP(end, 0x1000);
       address += 0x1000) {

    if (address >= (uintptr_t)&__start_READONLY && address <= ALIGN_UP((uintptr_t)&__stop_READONLY, 0x1000))
      mem_map_page(space_handle, address, KERNVIRT2PHYS(address), flags & ~MEM_PE_RW);
    else
      mem_map_page(space_handle, address, KERNVIRT2PHYS(address), flags);
  }

  // Map allocator bookkeeping area
  assert(user_space_allocator);

  for (uintptr_t phys_address = user_space_allocator->bookkeeping_phys_region().start;
       phys_address < user_space_allocator->bookkeeping_phys_region().end;
       phys_address += 0x1000) {
    mem_map_page(space_handle, PHYS2KERNVIRT(phys_address), phys_address, flags);
  }
}

mem_area mem_map_linear(mem_space space_handle,
                        uintptr_t start,
                        uintptr_t end,
                        uintptr_t phys_start,
                        uint8_t flags)
{
  space_info *space = &spaces[space_handle];
  uint16_t map_handle = space->linear_maps.push_back({phys_start});
  return space->areas.push_back({start, end, AREA_LINEAR_MAP, flags, map_handle});
}

mem_area mem_map_alloc(mem_space space_handle, uintptr_t start, uintptr_t end, uint8_t flags)
{
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
                    uint8_t flags)
{
  space_info &space = spaces[space_handle];
  uint16_t map_handle = space.file_maps.push_back({fd, offset, file_size});
  return space.areas.push_back({start, end, AREA_FILE, flags, map_handle});
}

static p2::optional<mem_area> find_area(mem_space space_handle, uintptr_t address)
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

typedef void (*page_fault_handler)(area_info &area, uintptr_t faulted_address);

static void page_fault_linear_map(area_info &area, uintptr_t faulted_address)
{
  linear_map_info &lm_info = spaces[current_space].linear_maps[area.info_handle];
  uintptr_t page_address = ALIGN_DOWN(faulted_address, 0x1000);
  ptrdiff_t area_offset = page_address - area.start;
  dbg_puts(mem, "linear map; mapping %x to %x", page_address, lm_info.phys_start + area_offset);
  mem_map_page(current_space, page_address, lm_info.phys_start + area_offset, area.flags);
}

static void page_fault_alloc(area_info &area, uintptr_t faulted_address)
{
  uintptr_t page_address = ALIGN_DOWN(faulted_address, 0x1000);
  uintptr_t phys_block = (uintptr_t)alloc_page();
  dbg_puts(mem, "alloc map; allocated %x and mapping it at %x", phys_block, page_address);
  mem_map_page(current_space, page_address, phys_block, area.flags);
  memset((void *)page_address, 0, 0x1000);
}

static void page_fault_file(area_info &area, uintptr_t faulted_address)
{
  uintptr_t page_address = ALIGN_DOWN(faulted_address, 0x1000);
  uintptr_t phys_block = (uintptr_t)alloc_page();
  dbg_puts(mem, "file map; allocated %x and mapping it at %x", phys_block, page_address);
  mem_map_page(current_space, page_address, phys_block, area.flags);
  memset((void *)page_address, 0, 0x1000);

  file_map_info &fm_info = spaces[current_space].file_maps[area.info_handle];
  ptrdiff_t area_offset = page_address - area.start;
  uint32_t file_offset = area_offset + fm_info.offset;

  // TODO: syscall here is unnecessary; we're already in the kernel
  if (syscall3(seek, fm_info.fd, file_offset, SEEK_BEG) < 0) {
    panic("failed to seek");
  }

  uint32_t read_count = p2::min<uint32_t>(fm_info.offset + fm_info.size - file_offset, 0x1000);
  dbg_puts(mem, "Reading max %d bytes", read_count);
  if (syscall3(read, fm_info.fd, (char *)page_address, read_count) < 0) {
    panic("failed to read");
  }
}

static void free_alloc_area(space_info &space, area_info &area)
{
  assert(area.type == AREA_ALLOC || area.type == AREA_FILE);

  for (uintptr_t page_address = area.start;
       page_address < area.end;
       page_address += 0x1000) {

    if (auto *pte = find_pte(space, page_address); pte && pte->flags & MEM_PE_P) {
      dbg_puts(mem, "free allocated page virt %x (%x)", page_address, pte_frame(pte));
      free_page((void *)pte_frame(pte));
      pte->flags &= ~MEM_PE_P;
    }
  }
}

extern "C" void int_page_fault(isr_registers regs)
{
  uint32_t faulted_address = 0;
  asm volatile("mov eax, cr2" : "=a"(faulted_address));

  if (regs.error_code & 1) {
    dbg_puts(mem, "process tried to access protected page at %x", faulted_address);
    proc_kill(proc_current_pid(), 15);
    proc_yield();
    return;
  }

  p2::optional<mem_area> area_handle = find_area(current_space, faulted_address);

  if (!area_handle) {
    dbg_puts(mem, "process tried to access un-mapped area at %x", faulted_address);
    proc_kill(proc_current_pid(), 15);
    proc_yield();
    return;
  }

  dbg_puts(mem, "page fault (error %x) at %x, area %d", regs.error_code, faulted_address, *area_handle);

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
             flags | MEM_PE_P|MEM_PE_RW|MEM_PE_U);
  // TODO: handle errors
  return 0;
}

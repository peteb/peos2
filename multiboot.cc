#include "multiboot.h"
#include "support/utils.h"
#include "memareas.h"

multiboot_info *multiboot_header = nullptr;

const char *multiboot_memory_type(uint32_t type) {
  switch (type) {
  case MULTIBOOT_MEMORY_AVAILABLE:
    return "available";

  case MULTIBOOT_MEMORY_RESERVED:
    return "reserved";

  case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
    return "acpi reclaimable";

  case MULTIBOOT_MEMORY_NVS:
    return "nvs";

  case MULTIBOOT_MEMORY_BADRAM:
    return "badram";
  }

  return "n/a";
}


uintptr_t multiboot_last_address() {
  multiboot_info *mbhdr = multiboot_header;
  uintptr_t last = 0;

  // Space for module metadata
  last = p2::max(last, PHYS2KERNVIRT(mbhdr->mods_addr + sizeof(multiboot_mod) * mbhdr->mods_count));

  multiboot_mod *mods = (multiboot_mod *)PHYS2KERNVIRT(mbhdr->mods_addr);
  for (size_t i = 0; i < mbhdr->mods_count; ++i) {
    // Make room for the module binary
    last = p2::max(last, PHYS2KERNVIRT(mods[i].mod_end));
  }

  return last;
}

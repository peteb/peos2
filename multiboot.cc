#include "multiboot.h"

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

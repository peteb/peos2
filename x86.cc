#include "x86.h"
#include "assert.h"

gdt_descriptor::gdt_descriptor(uint32_t base,
                               uint32_t limit,
                               uint8_t flags,
                               uint8_t type) {

  assert(limit <= 0x000FFFFF && "limit can only be 20 bits");

  this->base_0_15 = base & 0xFFFF;
  this->base_16_23 = (base >> 16) & 0xFF;
  this->base_24_31 = (base >> 24) & 0xFF;
  this->limit_0_15 = limit & 0xFFFF;
  this->limit_16_19 = (limit >> 16) & 0xF;
  this->flags = flags;
  this->type = type;
}

#include <stdint.h>

#include "pci.h"
#include "x86.h"
#include "debug.h"
#include "support/pool.h"

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA    0xCFC

static p2::pool<pci_device, 32> devices;

static inline uint32_t loc2addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t reg)
{
  return ((uint32_t)bus << 16) |
    ((uint32_t)slot << 11) |
    ((uint32_t)func << 8) |
    (reg << 2) |
    (uint32_t)0x80000000;
}

static uint32_t read_config(uint32_t address)
{
  outd(CONFIG_ADDRESS, address);
  return ind(CONFIG_DATA);
}

static void write_config(uint32_t address, uint32_t value)
{
  outd(CONFIG_ADDRESS, address);
  return outd(CONFIG_DATA, value);
}

void pci_init()
{
  for (uint8_t bus = 0; bus < 0xFF; ++bus) {
    for (uint8_t slot = 0; slot < 0xFF; ++slot) {
      uint32_t reg0 = read_config(loc2addr(bus, slot, 0, 0));
      uint16_t vendor = reg0 & 0xFFFF;

      if (vendor == 0xFFFF) {
        continue;
      }

      uint32_t reg3 = read_config(loc2addr(bus, slot, 0, 3));
      uint8_t header_type = (reg3 >> 16) & 0xFF;

      if ((header_type & 0x7F) == 0) {
        uint16_t device = reg0 >> 16;
        uint32_t bar0 = read_config(loc2addr(bus, slot, 0, 4));

        pci_device dev;
        dev.bus = bus;
        dev.slot = slot;
        dev.vendor = vendor;
        dev.device = device;
        dev.irq = read_config(loc2addr(bus, slot, 0, 0xF)) & 0xFF;

        if (bar0 & 1) {
          dev.mmio = false;
          dev.iobase = bar0 & ~3;
        }
        else {
          assert((bar0 & 0x6) == 0 && "only supports 32 bit BARs");
          dev.mmio = true;
          dev.iobase = bar0 & ~0xF;
        }

        const char *mmio = (dev.mmio ? "MMIO" : "  IO");
        log(pci, "%d/%d: vend=%x dev=%x io=%x %s irq=%d",
            bus,
            slot,
            vendor,
            device,
            dev.iobase,
            mmio,
            dev.irq);

        devices.push_back(dev);
      }
    }
  }
}

pci_device *pci_find_device(uint16_t vendor, uint16_t device)
{
  for (size_t i = 0; i < devices.watermark(); ++i) {
    if (!devices.valid(i))
      continue;

    if (devices[i].vendor == vendor && devices[i].device == device)
      return &devices[i];
  }

  return nullptr;
}

uint16_t pci_read_command(pci_device *dev)
{
  uint32_t reg1 = read_config(loc2addr(dev->bus, dev->slot, 0, 1));
  return reg1 & 0xFFFF;
}

void pci_write_command(pci_device *dev, uint16_t value)
{
  uint32_t address = loc2addr(dev->bus, dev->slot, 0, 1);
  uint32_t reg1 = read_config(address);
  reg1 = ((reg1 & 0xFFFF0000) | (value & 0xFFFF));
  write_config(address, reg1);
}

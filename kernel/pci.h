// -*- c++ -*-

#ifndef PEOS2_PCI_H
#define PEOS2_PCI_H

#include <stdint.h>

struct pci_device {
  uint8_t bus;
  uint8_t slot;

  uint16_t vendor;
  uint16_t device;
  uintptr_t iobase;
  bool mmio;
  uint8_t irq;
};

void pci_init();

//
// pci_find_device - finds the first matching device
//
pci_device *pci_find_device(uint16_t vendor, uint16_t device);
uint16_t    pci_read_command(pci_device *device);
void        pci_write_command(pci_device *device, uint16_t value);

#endif // !PEOS2_PCI_H

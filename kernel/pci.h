// -*- c++ -*-

#ifndef PEOS2_PCI_H
#define PEOS2_PCI_H

#include <stdint.h>

struct pci_device {
  uint16_t vendor;
  uint16_t device;
  uintptr_t iobase;
  bool mmio;
};

void        pci_init();
pci_device *pci_find_device(uint16_t vendor, uint16_t device);

#endif // !PEOS2_PCI_H

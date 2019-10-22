// -*- c++ -*-

#ifndef PEOS2_PROTECTED_MODE_H
#define PEOS2_PROTECTED_MODE_H

#include <stdint.h>

#define KERNEL_CODE_SEL GDT_SEGSEL(0, 1)
#define KERNEL_DATA_SEL GDT_SEGSEL(0, 2)
#define USER_CODE_SEL   GDT_SEGSEL(3, 3)
#define USER_DATA_SEL   GDT_SEGSEL(3, 4)
#define TSS_SEL         GDT_SEGSEL(3, 5)

void enter_protected_mode();
void tss_set_kernel_stack(uint32_t esp);
extern "C" void enter_ring3(uint16_t data_selector, uint16_t code_selector);

#endif // !PEOS2_PROTECTED_MODE_H

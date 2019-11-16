// -*- c++ -*-

#ifndef PEOS2_PROTECTED_MODE_H
#define PEOS2_PROTECTED_MODE_H

#include <stdint.h>
#include "x86.h"

#define KERNEL_CODE_SEL GDT_SEGSEL(0, 1)
#define KERNEL_DATA_SEL GDT_SEGSEL(0, 2)  // Hard-coded reference in boot.s
#define USER_CODE_SEL   GDT_SEGSEL(3, 3)
#define USER_DATA_SEL   GDT_SEGSEL(3, 4)
#define TSS_SEL         GDT_SEGSEL(3, 5)

extern "C" void enter_user_mode(uint16_t data_selector, uint16_t code_selector);

void enter_protected_mode();
void tss_set_kernel_stack(uint32_t esp);

#endif // !PEOS2_PROTECTED_MODE_H

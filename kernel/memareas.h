#ifndef PEOS2_MEMAREAS_H
#define PEOS2_MEMAREAS_H

#define USER_SPACE_STACK_BASE   0xB0000000  // User stack initial SP (growing down)
#define KERNEL_VIRTUAL_BASE     0xC0000000  // Code and data for kernel
#define PROC_KERNEL_STACK_BASE  0xD0000000  // Process' kernel stack initial SP (growing down)
#define KERNEL_SCRATCH_BASE     0xE0000000  // Temporary mappings

#define PHYS2KERNVIRT(value) ((value) + KERNEL_VIRTUAL_BASE)
#define KERNVIRT2PHYS(value) ((value) - KERNEL_VIRTUAL_BASE)

#endif // !PEOS2_MEMAREAS_H

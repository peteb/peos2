// -*- c++ -*-

#ifndef PEOS2_SYSCALLS_H
#define PEOS2_SYSCALLS_H

#include <stdint.h>
#include "syscall_decls.h"

typedef uint32_t (*syscall_fun)(...);

void syscalls_init();
void syscall_register(int num, syscall_fun handler);

#endif // !PEOS2_SYSCALLS_H

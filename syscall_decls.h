//
// Contains the whole syscall interface and should be used both by
// clients but also the kernel so that constants are in sync.
//
#ifndef PEOS2_SYSCALL_DECLS_H
#define PEOS2_SYSCALL_DECLS_H

#include "syscall_macros.h"

// Syscall numbers
#define SYSCALL_NUM_WRITE        1
#define SYSCALL_NUM_READ         2
#define SYSCALL_NUM_OPEN         3
#define SYSCALL_NUM_CONTROL      4

#define SYSCALL_NUM_YIELD       10
#define SYSCALL_NUM_EXIT        11
#define SYSCALL_NUM_KILL        12

// Flags
#define FLAG_OPEN_READWRITE   0x01
#define FLAG_OPEN_CREATE      0x02

// Control numbers
#define CTRL_RAMFS_SET_FILE_RANGE 0x0100      // (start_addr, size)

// Filesystem definitions
SYSCALL_DEF3(write,   SYSCALL_NUM_WRITE, int, const char *, int);
SYSCALL_DEF3(read,    SYSCALL_NUM_READ, int, char *, int);
SYSCALL_DEF2(open,    SYSCALL_NUM_OPEN, const char *, uint32_t);
SYSCALL_DEF4(control, SYSCALL_NUM_CONTROL, int, uint32_t, uint32_t, uint32_t);

#endif // !PEOS2_SYSCALL_DECLS_H

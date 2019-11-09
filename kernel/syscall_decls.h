//
// Contains the whole syscall interface and should be used both by
// clients but also the kernel so that constants are in sync.
//
#ifndef PEOS2_SYSCALL_DECLS_H
#define PEOS2_SYSCALL_DECLS_H

#include <stdint.h>
#include "syscall_macros.h"

// Syscall numbers
#define SYSCALL_NUM_WRITE        1
#define SYSCALL_NUM_READ         2
#define SYSCALL_NUM_OPEN         3
#define SYSCALL_NUM_CONTROL      4
#define SYSCALL_NUM_CLOSE        5
#define SYSCALL_NUM_SEEK         6
#define SYSCALL_NUM_TELL         7
#define SYSCALL_NUM_MKDIR        8

#define SYSCALL_NUM_YIELD       10
#define SYSCALL_NUM_EXIT        11
#define SYSCALL_NUM_KILL        12
#define SYSCALL_NUM_SPAWN       13

#define SYSCALL_NUM_MMAP        20

// Flags
#define FLAG_OPEN_READWRITE   0x01
#define FLAG_OPEN_CREATE      0x02

#define SEEK_CUR              1
#define SEEK_BEG              2

// Errors
#define EINVOP       -100  // Invalid operation
#define ENOENT       -200  // Some component of the given path is missing
#define ENODIR       -201  // Path parent isn't a directory

// Control numbers
#define CTRL_RAMFS_SET_FILE_RANGE 0x0100      // (start_addr, size)
#define CTRL_RAMFS_GET_FILE_RANGE 0x0200      // (*start_addr, *size)

// Filesystem definitions
SYSCALL_DEF3(write,   SYSCALL_NUM_WRITE, int, const char *, int);
SYSCALL_DEF3(read,    SYSCALL_NUM_READ, int, char *, int);
SYSCALL_DEF2(open,    SYSCALL_NUM_OPEN, const char *, uint32_t);
SYSCALL_DEF1(close,   SYSCALL_NUM_CLOSE, int);
SYSCALL_DEF4(control, SYSCALL_NUM_CONTROL, int, uint32_t, uint32_t, uint32_t);
SYSCALL_DEF3(seek,    SYSCALL_NUM_SEEK, int, int, int);
SYSCALL_DEF2(tell,    SYSCALL_NUM_TELL, int, int *);
SYSCALL_DEF1(mkdir,   SYSCALL_NUM_MKDIR, const char *);

// Process definitions
SYSCALL_DEF1(exit,    SYSCALL_NUM_EXIT, int);
SYSCALL_DEF1(spawn,   SYSCALL_NUM_SPAWN, const char *);

// Memory definitions
SYSCALL_DEF5(mmap,    SYSCALL_NUM_MMAP, void *, void *, int, uint32_t, uint8_t);

#endif // !PEOS2_SYSCALL_DECLS_H
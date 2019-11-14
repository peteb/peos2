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
#define SYSCALL_NUM_DUP2         9

#define SYSCALL_NUM_YIELD       10
#define SYSCALL_NUM_EXIT        11
#define SYSCALL_NUM_KILL        12
#define SYSCALL_NUM_SPAWN       13
#define SYSCALL_NUM_EXEC        14
#define SYSCALL_NUM_FORK        15

#define SYSCALL_NUM_MMAP        20

// Flags
#define OPEN_READ             0x01
#define OPEN_READWRITE        0x02
#define OPEN_CREATE           0x04
#define OPEN_RETAIN_EXEC      0x10  // Keep the fd open after `exec`

#define SEEK_CUR              1
#define SEEK_BEG              2

// Errors
#define ENOSUPPORT   -100  // Invalid operation/not supported
#define ENOENT       -200  // Some component of the given path is missing
#define ENODIR       -201  // Path parent isn't a directory
#define ENODRIVER    -202  // Path doesn't contain a mapped driver
#define ENOSPACE     -203  // No space in structure or file
#define EINCONSTATE  -204  // Internal state is inconsistent

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
SYSCALL_DEF2(dup2,    SYSCALL_NUM_DUP2, int, int);

// Process definitions
SYSCALL_DEF1(exit,    SYSCALL_NUM_EXIT, int);
SYSCALL_DEF1(spawn,   SYSCALL_NUM_SPAWN, const char *);
SYSCALL_DEF0(fork,    SYSCALL_NUM_FORK);

//
// exec - rewrites the current process so that it'll run `filename`
// @filename: path to an ELF executable
// @argv: null-terminated list of pointers to arguments
//
// - file descriptors not marked RETAIN_EXEC will be closed
// - memory areas not marked RETAIN_EXEC will be removed and backing
//   pages might be freed
// - user stack will be reset and pages might be freed
// - kernel stack remains -- it's used for the syscall invocation
// - there's no way to "share" data between before and after exec
//   without using some other syscall; the old data is gone
//
SYSCALL_DEF2(exec, SYSCALL_NUM_EXEC, const char *, const char **);


// Memory definitions
SYSCALL_DEF5(mmap,    SYSCALL_NUM_MMAP, void *, void *, int, uint32_t, uint8_t);

#endif // !PEOS2_SYSCALL_DECLS_H

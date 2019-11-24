//
// Contains the whole syscall interface and should be used both by
// clients but also the kernel so that constants are in sync.
//
#ifndef PEOS2_SYSCALL_DECLS_H
#define PEOS2_SYSCALL_DECLS_H

#include <stdint.h>
#include "syscall_macros.h"

// Syscall numbers
#define SYSCALL_NUM_STRERROR     50

#define SYSCALL_NUM_WRITE       100
#define SYSCALL_NUM_READ        101
#define SYSCALL_NUM_OPEN        103
#define SYSCALL_NUM_CONTROL     104
#define SYSCALL_NUM_CLOSE       105
#define SYSCALL_NUM_SEEK        106
#define SYSCALL_NUM_TELL        107
#define SYSCALL_NUM_MKDIR       108
#define SYSCALL_NUM_DUP2        109

#define SYSCALL_NUM_YIELD       200
#define SYSCALL_NUM_EXIT        201
#define SYSCALL_NUM_KILL        202
#define SYSCALL_NUM_EXEC        204
#define SYSCALL_NUM_FORK        205
#define SYSCALL_NUM_SHUTDOWN    206
#define SYSCALL_NUM_WAIT        207
#define SYSCALL_NUM_SET_TIMEOUT 208

#define SYSCALL_NUM_MMAP        300

#define SYSCALL_NUM_MAX         999

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
#define EINVVAL      -205  // Invalid value
#define EBUSY        -206  // Resource busy
#define ETIMEOUT     -207  // Timed out

// Control numbers
#define CTRL_NET_HW_ADDR          0x0010      // uint8[6]
#define CTRL_RAMFS_SET_FILE_RANGE 0x0100      // (start_addr, size)
#define CTRL_RAMFS_GET_FILE_RANGE 0x0200      // (*start_addr, *size)


// System definitions
SYSCALL_DEF3(strerror,  SYSCALL_NUM_STRERROR, int, char *, int);

// Filesystem definitions
SYSCALL_DEF3(write,       SYSCALL_NUM_WRITE, int, const char *, int);
SYSCALL_DEF3(read,        SYSCALL_NUM_READ, int, char *, int);
SYSCALL_DEF2(open,        SYSCALL_NUM_OPEN, const char *, uint32_t);
SYSCALL_DEF1(close,       SYSCALL_NUM_CLOSE, int);
SYSCALL_DEF4(control,     SYSCALL_NUM_CONTROL, int, uint32_t, uint32_t, uint32_t);
SYSCALL_DEF3(seek,        SYSCALL_NUM_SEEK, int, int, int);
SYSCALL_DEF2(tell,        SYSCALL_NUM_TELL, int, int *);
SYSCALL_DEF1(mkdir,       SYSCALL_NUM_MKDIR, const char *);
SYSCALL_DEF2(dup2,        SYSCALL_NUM_DUP2, int, int);

// Process definitions
SYSCALL_DEF0(yield,       SYSCALL_NUM_YIELD);
SYSCALL_DEF1(exit,        SYSCALL_NUM_EXIT, int);
SYSCALL_DEF0(fork,        SYSCALL_NUM_FORK);
SYSCALL_DEF0(shutdown,    SYSCALL_NUM_SHUTDOWN);
SYSCALL_DEF1(wait,        SYSCALL_NUM_WAIT, int);
SYSCALL_DEF1(set_timeout, SYSCALL_NUM_SET_TIMEOUT, int);

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


// Structs
typedef struct {
  char name[64];
} dirent_t;

#endif // !PEOS2_SYSCALL_DECLS_H

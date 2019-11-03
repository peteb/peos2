// -*- c++ -*-

#ifndef PEOS2_SYSCALLS_H
#define PEOS2_SYSCALLS_H

#include <stdint.h>

#define SYSCALL_NUM_WRITE        1
#define SYSCALL_NUM_READ         2
#define SYSCALL_NUM_OPEN         3

#define SYSCALL_NUM_YIELD       10
#define SYSCALL_NUM_EXIT        11
#define SYSCALL_NUM_KILL        12

// These macros are for the client, the solution is inspired by JamesM

// It's important to clobber memory, otherwise GCC might optimize away
// referenced data.

#define SYSCALL_DEF0(name, num)                           \
static int _syscall_##name() {                            \
  int ret;                                                \
  asm volatile("int 0x90"                                 \
               : "=a"(ret)                                \
               : "a"(num));                               \
  return ret;                                             \
}

#define SYSCALL_DEF1(name, num, P1)                       \
static int _syscall_##name(P1 p1) {                       \
  int ret;                                                \
  asm volatile("int 0x90"                                 \
               : "=a"(ret)                                \
               : "a"(num),                                \
                 "b"(p1)                                  \
               : "memory");                               \
  return ret;                                             \
}

#define SYSCALL_DEF2(name, num, P1, P2)                   \
static int _syscall_##name(P1 p1, P2 p2) {                \
  int ret;                                                \
  asm volatile("int 0x90"                                 \
               : "=a"(ret)                                \
               : "a"(num),                                \
                 "b"(p1),                                 \
                 "c"(p2)                                  \
               : "memory");                               \
  return ret;                                             \
}

#define SYSCALL_DEF3(name, num, P1, P2, P3)               \
static int _syscall_##name(P1 p1, P2 p2, P3 p3) {         \
  int ret;                                                \
  asm volatile("int 0x90"                                 \
               : "=a"(ret)                                \
               : "a"(num),                                \
                 "b"(p1),                                 \
                 "c"(p2),                                 \
                 "d"(p3)                                  \
               : "memory");                               \
  return ret;                                             \
}

#define SYSCALL0(name) _syscall_##name()
#define SYSCALL1(name, p1) _syscall_##name(p1)
#define SYSCALL2(name, p1, p2) _syscall_##name(p1, p2)
#define SYSCALL3(name, p1, p2, p3) _syscall_##name(p1, p2, p3)

typedef uint32_t (*syscall_fun)(...);

void syscalls_init();
void syscall_register(int num, syscall_fun handler);

#endif // !PEOS2_SYSCALLS_H

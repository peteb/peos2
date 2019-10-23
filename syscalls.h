// -*- c++ -*-

#ifndef PEOS2_SYSCALLS_H
#define PEOS2_SYSCALLS_H

// These macros are for the client, the solution is inspired by JamesM

// It's important to clobber memory, otherwise GCC might optimize away
// referenced data.

#define SYSCALL_DEF0(name, num)                           \
static int syscall_##name() {                             \
  int ret;                                                \
  asm volatile("int 0x90"                                 \
               : "=a"(ret)                                \
               : "a"(num));                               \
  return ret;                                             \
}

#define SYSCALL_DEF1(name, num, P1)                       \
static int syscall_##name(P1 p1) {                        \
  int ret;                                                \
  asm volatile("int 0x90"                                 \
               : "=a"(ret)                                \
               : "a"(num),                                \
                 "b"(p1)                                  \
               : "memory");                               \
  return ret;                                             \
}

#define SYSCALL0(name) syscall_##name()
#define SYSCALL1(name, p1) syscall_##name(p1)

void syscalls_init();

#endif // !PEOS2_SYSCALLS_H

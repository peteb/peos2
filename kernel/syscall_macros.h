//
// These macros are for the client, solution inspired by JamesM
//
// It's important to clobber memory, otherwise GCC might optimize away
// referenced data.
//

#ifndef PEOS2_SYSCALL_MACROS_H
#define PEOS2_SYSCALL_MACROS_H

// TODO: why do we have to clobber ESI? The ISR is using PUSHAD!

#define SYSCALL_DEF0(name, num)                                  \
inline static int _syscall_##name()                              \
{                                                                \
  int ret;                                                       \
  asm volatile("int 0x90"                                        \
               : "=a"(ret)                                       \
               : "a"(num)                                        \
               : "memory", "esi", "edi");                        \
  return ret;                                                    \
}

#define SYSCALL_DEF1(name, num, P1)                              \
inline static int _syscall_##name(P1 p1)                         \
{                                                                \
  int ret;                                                       \
  asm volatile("int 0x90"                                        \
               : "=a"(ret)                                       \
               : "a"(num),                                       \
                 "b"(p1)                                         \
               : "memory", "esi", "edi");                        \
  return ret;                                                    \
}

#define SYSCALL_DEF2(name, num, P1, P2)                          \
inline static int _syscall_##name(P1 p1, P2 p2)                  \
{                                                                \
  int ret;                                                       \
  asm volatile("int 0x90"                                        \
               : "=a"(ret)                                       \
               : "a"(num),                                       \
                 "b"(p1),                                        \
                 "c"(p2)                                         \
               : "memory", "esi", "edi");                        \
  return ret;                                                    \
}

#define SYSCALL_DEF3(name, num, P1, P2, P3)                      \
inline static int _syscall_##name(P1 p1, P2 p2, P3 p3)           \
{                                                                \
  int ret;                                                       \
  asm volatile("int 0x90"                                        \
               : "=a"(ret)                                       \
               : "a"(num),                                       \
                 "b"(p1),                                        \
                 "c"(p2),                                        \
                 "d"(p3)                                         \
               : "memory", "esi", "edi");                        \
  return ret;                                                    \
}

#define SYSCALL_DEF4(name, num, P1, P2, P3, P4)                  \
inline static int _syscall_##name(P1 p1, P2 p2, P3 p3, P4 p4)    \
{                                                                \
  int ret;                                                       \
  asm volatile("int 0x90"                                        \
               : "=a"(ret)                                       \
               : "a"(num),                                       \
                 "b"(p1),                                        \
                 "c"(p2),                                        \
                 "d"(p3),                                        \
                 "S"(p4)                                         \
               : "memory", "edi");                               \
  return ret;                                                    \
}

#define SYSCALL_DEF5(name, num, P1, P2, P3, P4, P5)                  \
inline static int _syscall_##name(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) \
{                                                                    \
  int ret;                                                           \
  asm volatile("int 0x90"                                            \
               : "=a"(ret)                                           \
               : "a"(num),                                           \
                 "b"(p1),                                            \
                 "c"(p2),                                            \
                 "d"(p3),                                            \
                 "S"(p4),                                            \
                 "D"(p5)                                             \
               : "memory");                                          \
  return ret;                                                        \
}

#define syscall0(name) _syscall_##name()
#define syscall1(name, p1) _syscall_##name(p1)
#define syscall2(name, p1, p2) _syscall_##name(p1, p2)
#define syscall3(name, p1, p2, p3) _syscall_##name(p1, p2, p3)
#define syscall4(name, p1, p2, p3, p4) _syscall_##name(p1, p2, p3, p4)
#define syscall5(name, p1, p2, p3, p4, p5) _syscall_##name(p1, p2, p3, p4, p5)

#endif // !PEOS2_SYSCALL_MACROS_H

#ifndef PEOS2_SUPPORT_USERSPACE_H
#define PEOS2_SUPPORT_USERSPACE_H

#include <kernel/syscall_decls.h>

#define START(fun) extern "C" void _start(int argc, char *argv[])    \
  {                                                                  \
    syscall1(exit, fun(argc, argv));                                 \
  }


#endif // !PEOS2_SUPPORT_USERSPACE_H

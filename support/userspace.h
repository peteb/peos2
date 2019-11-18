#ifndef PEOS2_SUPPORT_USERSPACE_H
#define PEOS2_SUPPORT_USERSPACE_H

#include <kernel/syscall_decls.h>

#define START(fun) extern "C" void _start(int argc, char *argv[])    \
  {                                                                  \
    syscall1(exit, fun(argc, argv));                                 \
  }


#ifdef __cplusplus

#include <support/format.h>
#include <kernel/syscall_decls.h>

namespace p2 {
  static inline int verify(int retval)
  {
    if (retval >= 0)
      return retval;

    // TODO: support negative values in format
    p2::format<128> msg("call failed with code %d", (uintptr_t)retval);
    syscall3(write, 2, msg.str().c_str(), msg.str().size());
    syscall1(exit, 1);
    return 1;
  }

  template<int N>
  static inline void puts(int fd, const p2::format<N> &fm)
  {
    verify(syscall3(write, fd, fm.str().c_str(), fm.str().size()));
  }



}
#endif

#endif // !PEOS2_SUPPORT_USERSPACE_H

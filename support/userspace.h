#ifndef PEOS2_SUPPORT_USERSPACE_H
#define PEOS2_SUPPORT_USERSPACE_H

#include <kernel/syscall_decls.h>
#include <support/utils.h>

#define START(fun) extern "C" void _start(int argc, char *argv[])    \
  {                                                                  \
    syscall1(exit, fun(argc, argv));                                 \
  }                                                                  \
  void panic(const char *explanation)                                \
  {                                                                  \
    syscall3(write, 0, explanation, strlen(explanation));            \
    while (true);                                                    \
  }


static inline void puts(const char *message)
{
  syscall3(write, 0, message, strlen(message));
  syscall3(write, 0, "\n", 1);
}

static inline void print(const char *message)
{
  syscall3(write, 0, message, strlen(message));
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


static inline int list_dir(int fd, dirent_t *dirents, size_t count)
{
  int bytes_read = 0;
  char *out = (char *)dirents;
  size_t bytes_left = count * sizeof(*dirents);

  while ((bytes_read = syscall3(read, fd, out, bytes_left)) > 0) {
    bytes_left -= bytes_read;
    out += bytes_read;
  }

  if (bytes_read < 0)
    return bytes_read;

  return (out - (char *)dirents) / sizeof(*dirents);
}

#endif // !PEOS2_SUPPORT_USERSPACE_H

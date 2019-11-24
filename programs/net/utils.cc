#include <kernel/syscall_decls.h>

#include "utils.h"

char debug_out_buffer[512];

int read(int fd, char *buf, size_t length)
{
  size_t total = 0;
  int ret;

  while ((ret = syscall3(read, fd, buf, length)) > 0) {
    length -= ret;
    total += ret;
  }

  if (ret < 0)
    return ret;
  else
    return total;
}

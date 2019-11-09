#include "kernel/syscall_decls.h"

extern "C" int _start() {
  static int hej;
  hej++;

  int fd = syscall2(open, "/dev/term1", 0);

  const char *message = ">>> Hello World! <<<\n";
  syscall3(write, fd, message, 21);
  return 555 + hej;
}

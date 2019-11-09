#include "kernel/syscall_decls.h"

extern "C" int _start() {
  static int hej;
  hej++;

  int fd = SYSCALL2(open, "/dev/term1", 0);

  const char *message = ">>> Hello World! <<<\n";
  SYSCALL3(write, fd, message, 21);
  return 555 + hej;
}

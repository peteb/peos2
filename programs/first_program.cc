#include "kernel/syscall_decls.h"

int main();

extern "C" void _start() {
  int ret = main();
  syscall1(exit, ret);
}


int main() {
  static int hej;
  hej++;

  int fd = syscall2(open, "/dev/term1", 0);

  const char *message = ">>> Hello World! <<<\n";
  syscall3(write, fd, message, 21);

  return 555 + hej;
}

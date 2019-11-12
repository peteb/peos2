#include "kernel/syscall_decls.h"

int main(int argc, const char *argv[]);

extern "C" void _start(int argc, const char *argv[]) {
  int ret = main(argc, argv);
  syscall1(exit, ret);
}


int main(int argc, const char *argv[]) {
  static int hej;
  hej++;

  int fd = syscall2(open, "/dev/term1", 0);

  const char *message = ">>> Hello World! <<<\n";
  syscall3(write, fd, message, 21);

  for (int i = 0; i < argc; ++i) {
    syscall3(write, fd, argv[i], 15);
    syscall3(write, fd, "\n", 1);
  }

  return 555 + hej;
}

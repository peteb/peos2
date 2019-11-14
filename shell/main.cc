#include <support/string.h>
#include <support/userspace.h>
#include <kernel/syscall_decls.h>

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  int stdout = syscall2(open, "/dev/term0", 0);
  syscall3(write, stdout, "WELCOME TO SHELL\n", 17);
  return 0;
}

START(main);

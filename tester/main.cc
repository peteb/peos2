#include <support/string.h>
#include <support/userspace.h>
#include <kernel/syscall_decls.h>

int main(int, char *argv[]) {
  syscall3(write, 0, "WELCOME TO TESTER\n", 18);

  while (true) {
    syscall3(write, 0, argv[1], 1);
    syscall0(yield);
  }

  return 0;
}

START(main);

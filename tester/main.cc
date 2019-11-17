#include <support/string.h>
#include <support/userspace.h>
#include <kernel/syscall_decls.h>

int main(int, char *[]) {
  syscall3(write, 0, "WELCOME TO TESTER\n", 18);
  syscall0(shutdown);
  return 0;
}

START(main);

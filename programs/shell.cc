#include <support/string.h>
#include <support/userspace.h>
#include <kernel/syscall_decls.h>

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  syscall3(write, 0, "WELCOME TO SHELL\n", 17);

  while (true) {
    syscall3(write, 0, "> ", 2);
    char input[64];
    int bytes_read = syscall3(read, 1, input, sizeof(input));

    if (bytes_read < 0) {
      syscall3(write, 0, "Failed to read\n", 15);
    }

    syscall1(exit, 0);
  }

  return 0;
}

START(main);

#include <support/string.h>
#include <support/userspace.h>
#include <kernel/syscall_decls.h>

#include "command_line.h"

using namespace p2;

int main(int argc, char *argv[])
{
  assert(argc > 0);

  if (argc != 2) {
    puts(0, format<128>("Usage: %s dirpath\n", argv[0]));
    syscall1(exit, 1);
  }

  print(argv[1]);
  print(":\n");

  int fd = syscall2(open, argv[1], 0);

  if (fd < 0) {
    // TODO: print which error
    puts("Failed");
    syscall1(exit, 1);
  }

  dirent_t entries[32];
  int num_entries = list_dir(fd, entries, ARRAY_SIZE(entries));

  if (num_entries < 0) {
    // TODO: print which error
    puts("Failed");
    syscall1(exit, 1);
  }

  syscall1(close, fd);

  for (int i = 0; i < num_entries; ++i) {
    puts(entries[i].name);
  }

  return 0;
}

START(main);

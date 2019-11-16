#include <support/string.h>
#include <support/userspace.h>
#include <kernel/syscall_decls.h>

void panic(const char *explanation) {
  syscall3(write, 0, explanation, strlen(explanation));
  while (true);
}

static inline int list_dir(const char *filename, dirent_t *dirents, size_t count)
{
  int fd = syscall2(open, filename, 0);

  if (fd < 0)
    return fd;

  int bytes_read = 0;
  char *out = (char *)dirents;
  size_t bytes_left = count * sizeof(*dirents);

  while ((bytes_read = syscall3(read, fd, out, bytes_left)) > 0) {
    bytes_left -= bytes_read;
    out += bytes_read;
  }

  if (bytes_read < 0)
    return bytes_read;

  if (int retval = syscall1(close, fd); retval < 0)
    return retval;

  return (out - (char *)dirents) / sizeof(*dirents);
}


int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  syscall3(write, 0, "WELCOME TO SHELL\n", 17);

  dirent_t entries[16];
  int num_entries = list_dir("/dev/", entries, ARRAY_SIZE(entries));

  for (int i = 0; i < num_entries; ++i) {
    syscall3(write, 0, entries[i].name, strlen(entries[i].name));
    syscall3(write, 0, "\n", 1);
  }

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

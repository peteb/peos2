// shell_launcher.cc - program that starts shells on the /dev/termN terminals

#include <kernel/syscall_decls.h>

#include <support/userspace.h>
#include <support/format.h>
#include <support/utils.h>
#include <support/pool.h>

using namespace p2;

static void setup_std_fds(const char *terminal_filename);
static int list_dir(const char *filename, dirent_t *dirents, size_t count);
static void list_terminals(pool<string<32>, 16> *terminals);

int main(int, char *[])
{
  pool<string<32>, 16> terminals;
  list_terminals(&terminals);

  // TODO: enumerate terminals and spawn one shell for each
  pool<int, 16> child_pids;

  for (int i = 0; i < terminals.watermark(); ++i) {
    if (!terminals.valid(i))
      continue;

    const char *filename = terminals[i].c_str();
    puts(0, format<64>("starting shell for %s", filename));

    int child_pid = syscall0(fork);

    if (child_pid != 0) {
      child_pids.push_back(child_pid);
    }
    else {
      setup_std_fds(filename);
      const char *argv[] = {nullptr};
      verify(syscall2(exec, "/ramfs/bin/shell", argv));
    }
  }

  for (size_t i = 0; i < child_pids.watermark(); ++i) {
    // TODO: restart shells if they exit?
    syscall1(wait, child_pids[i]);
  }

  puts("All shells have terminated");

  return 0;
}

START(main);

static void setup_term_at(const char *filename, int dest_fd)
{
  int source_fd = verify(syscall2(open, filename, OPEN_RETAIN_EXEC));

  if (source_fd != dest_fd) {
    verify(syscall2(dup2, source_fd, dest_fd));
    verify(syscall1(close, source_fd));
  }
}

static void setup_std_fds(const char *terminal_filename)
{
  setup_term_at(terminal_filename, 0);
  setup_term_at(terminal_filename, 1);
  setup_term_at(terminal_filename, 2);
}

static int list_dir(const char *filename, dirent_t *dirents, size_t count)
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

void panic(const char *explanation) {
  syscall3(write, 0, explanation, strlen(explanation));
  while (true);
}

static void list_terminals(pool<string<32>, 16> *terminals)
{
  assert(terminals);

  dirent_t entries[16];
  int num_entries = verify(list_dir("/dev/", entries, ARRAY_SIZE(entries)));

  for (int i = 0; i < num_entries; ++i) {
    if (strncmp(entries[i].name, "term", 4) == 0 &&
        strncmp(entries[i].name, "term0", strlen(entries[i].name)) != 0) {
      p2::format<32> path("/dev/%s", entries[i].name);
      terminals->emplace_back(path.str());
    }
  }
}

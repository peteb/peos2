// shell_launcher.cc - program that starts shells on the /dev/termN terminals

#include <kernel/syscall_decls.h>

#include <support/userspace.h>
#include <support/format.h>
#include <support/utils.h>
#include <support/pool.h>

using namespace p2;

static void setup_std_fds(const char *terminal_filename);
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
    puts(0, format<64>("starting shell for %s\n", filename));

    int child_pid = syscall0(fork);

    if (child_pid != 0) {
      child_pids.push_back(child_pid);
    }
    else {
      setup_std_fds(filename);
      const char *argv[] = {"/ramfs/bin/shell", nullptr};
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

void panic(const char *explanation) {
  syscall3(write, 0, explanation, strlen(explanation));
  while (true);
}

static void list_terminals(pool<string<32>, 16> *terminals)
{
  assert(terminals);

  dirent_t entries[16];
  int dir_fd = verify(syscall2(open, "/dev/", 0));
  int num_entries = verify(list_dir(dir_fd, entries, ARRAY_SIZE(entries)));
  verify(syscall1(close, dir_fd));

  for (int i = 0; i < num_entries; ++i) {
    if (strncmp(entries[i].name, "term", 4) == 0 &&
        strncmp(entries[i].name, "term0", strlen(entries[i].name)) != 0) {
      p2::format<32> path("/dev/%s", entries[i].name);
      terminals->emplace_back(path.str());
    }
  }
}

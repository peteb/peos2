// shell_launcher.cc - program that starts shells on the /dev/termN terminals

#include <kernel/syscall_decls.h>

#include <support/userspace.h>
#include <support/format.h>
#include <support/utils.h>
#include <support/pool.h>

static void setup_std_fds(const char *terminal_filename);

using namespace p2;

void panic(const char *explanation) {
  syscall3(write, 0, explanation, strlen(explanation));
  while (true);
}

int main(int, char *[])
{
  // TODO: enumerate terminals and spawn one shell for each
  p2::pool<int, 16> child_pids;

  for (int i = 1; i < 5; ++i) {
    format<64> term_filename("/dev/term%d", i);
    puts(0, format<64>("starting shell for %s", term_filename.str().c_str()));

    int child_pid = syscall0(fork);

    if (child_pid != 0) {
      child_pids.push_back(child_pid);
    }
    else {
      setup_std_fds(term_filename.str().c_str());
      const char *argv[] = {nullptr};
      verify(syscall2(exec, "/ramfs/bin/shell", argv));
    }
  }

  for (size_t i = 0; i < child_pids.watermark(); ++i)
    syscall1(wait, child_pids[i]);

  {
    const char *msg = "All shells have terminated\n";
    syscall3(write, 0, msg, strlen(msg));
  }

  // TODO: restart shells if they exit? We need to at least wait for them
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

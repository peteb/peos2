#include "terminal.h"
#include "filesystem.h"
#include "screen.h"
#include "process.h"

#include "support/format.h"
#include "support/queue.h"
#include "support/string.h"

static int write(vfs_char_device *, const char *path, const char *data, int length);
static int read(vfs_char_device *device, const char *path, char *data, int length);

static p2::queue<char, 256> input_queue;
static p2::string<200> line_buffer;
static proc_handle waiting_process;
static bool process_waiting = false;

static vfs_device_driver interface =
{
  .write = write,
  .read = read
};

void term_init(const char *name) {
  vfs_node_handle term_driver = vfs_create_node(VFS_CHAR_DEVICE);
  vfs_set_driver(term_driver, &interface, nullptr);
  vfs_add_dirent(vfs_lookup("/dev/"), name, term_driver);
}

void term_keypress(uint16_t keycode) {
  if (keycode >= 0x7F) {
    // Special key
    return;
  }

  char buf[2] = {(char)keycode, '\0'};

  if (keycode == '\n') {
    if (line_buffer.size() >= line_buffer.capacity() - 1) {
      return;
    }

    line_buffer.append((char)(keycode & 0xFF));
    print(buf);

    for (int i = 0; i < line_buffer.size(); ++i) {
      input_queue.push_back(line_buffer[i]);
    }

    line_buffer.clear();

    if (process_waiting) {
      // TODO: clean up the waiting structure
      proc_resume(waiting_process);
      process_waiting = false;
      proc_switch(waiting_process);
    }
  }
  else if (keycode == '\r') {
    if (line_buffer.size() > 0) {
      screen_backspace();
      line_buffer.backspace();
    }
  }
  else {
    if (line_buffer.size() >= line_buffer.capacity() - 2) {
      return;
    }

    line_buffer.append((char)(keycode & 0xFF));
    print(buf);
  }
}

static int write(vfs_char_device *, const char *path, const char *data, int length) {
  assert(path[0] == '\0');  // No subpaths possible on terminal
  print(data, length);
  // TODO: parse and execute escape sequences
  return length;
}

static int read(vfs_char_device *, const char *path, char *data, int length) {
  assert(path[0] == '\0');
  int bytes_read = 0;

  while (true) {
    asm volatile("cli");

    if (input_queue.size() == 0) {
      // We need to block!
      proc_suspend(proc_current_pid());
      waiting_process = proc_current_pid();
      process_waiting = true;

      proc_yield();
      // When the process is unsuspended, execution will continue here
    }

    asm volatile("cli");

    // Even though we've been woken up, the input_queue might've been
    // emptied since then somehow. That's what we've got the outer
    // loop for.
    while (input_queue.size() > 0 && length > 0) {
      data[bytes_read++] = input_queue.pop_front();
      --length;
    }

    asm volatile("sti");

    if (bytes_read > 0) {
      break;
    }
  }

  return bytes_read;
}

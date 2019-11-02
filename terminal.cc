#include "terminal.h"
#include "filesystem.h"
#include "screen.h"
#include "process.h"

#include "support/format.h"
#include "support/queue.h"
#include "support/string.h"
#include "support/blocking_queue.h"

// Statics
static int write(vfs_char_device *, const char *path, const char *data, int length);
static int read(vfs_char_device *device, const char *path, char *data, int length);

// Global state
static p2::string<200> line_buffer;
static p2::blocking_data_queue<1000> input_queue;

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

    // Note: queue.push_back will switch to a waiting process
    // immediately
    size_t bytes_pushed = input_queue.push_back(&line_buffer[0], line_buffer.size(), [&]() {
                            line_buffer.clear();
                          });

    assert(bytes_pushed != 0);
  }
  else if (keycode == '\r') {
    if (line_buffer.size() > 0) {
      print("\r");
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
  return input_queue.pop_front(data, length);
}

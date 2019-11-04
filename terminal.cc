#include "terminal.h"
#include "filesystem.h"
#include "screen.h"
#include "process.h"

#include "support/format.h"
#include "support/pool.h"

#include "terminal_private.h"

// Statics
static int write(int handle, const char *data, int length);
static int read(int handle, char *data, int length);
static int open(vfs_device *device, const char *path, uint32_t flags);
static void focus_terminal(uint16_t term_id);

// Global state
static p2::pool<terminal, 16> terminals;
static uint16_t current_terminal = terminals.end();

// Definitions
void term_init(const char *name, screen_buffer buffer) {
  static vfs_device_driver interface =
  {
    .write = write,
    .read = read,
    .open = open,
    .control = nullptr
  };

  uintptr_t term_id = terminals.emplace_back(buffer);

  vfs_node_handle term_driver = vfs_create_node(VFS_CHAR_DEVICE);
  vfs_set_driver(term_driver, &interface, (void *)term_id);
  vfs_add_dirent(vfs_lookup("/dev/"), name, term_driver);

  if (current_terminal == terminals.end()) {
    current_terminal = term_id;
  }
}

void term_keypress(uint16_t keycode) {
  // TODO: all this code happens in the interrupt handler for the
  // keyboard. We probably don't want to do this much in there. It
  // might for example copy the whole screen!
  uint8_t special_code = keycode >> 8;
  if (special_code >= 1 && special_code <= 12) {
    // F1..F12
    focus_terminal(special_code - 1);
    return;
  }

  terminals[current_terminal].on_key(keycode);
}

static int write(int handle, const char *data, int length) {
  return terminals[handle].syscall_write(data, length);
}

static int read(int handle, char *data, int length) {
  return terminals[handle].syscall_read(data, length);
}

static int open(vfs_device *device, const char *path, uint32_t flags) {
  (void)flags;
  // TODO: handle flags
  assert(path[0] == '\0');
  return (int)vfs_get_opaque(device);
}

static void focus_terminal(uint16_t term_id) {
  terminals[term_id].focus();
  current_terminal = term_id;
}

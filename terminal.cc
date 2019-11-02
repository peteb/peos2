#include "terminal.h"
#include "filesystem.h"
#include "screen.h"
#include "process.h"

#include "support/format.h"
#include "support/pool.h"

#include "terminal_private.h"

// Statics
static int write(vfs_char_device *, const char *path, const char *data, int length);
static int read(vfs_char_device *device, const char *path, char *data, int length);
static void focus_terminal(uint16_t term_id);

// Global state
static p2::pool<terminal, 16> terminals;
static uint16_t current_terminal = terminals.end();

static vfs_device_driver interface =
{
  .write = write,
  .read = read
};

void term_init(const char *name, screen_buffer buffer) {
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

static int write(vfs_char_device *device, const char *path, const char *data, int length) {
  assert(path[0] == '\0');  // No subpaths possible on terminal
  return terminals[(uintptr_t)vfs_get_opaque(device)].syscall_write(data, length);
}

static int read(vfs_char_device *device, const char *path, char *data, int length) {
  assert(path[0] == '\0');
  return terminals[(uintptr_t)vfs_get_opaque(device)].syscall_read(data, length);
}

static void focus_terminal(uint16_t term_id) {
  terminals[term_id].focus();
  current_terminal = term_id;
}

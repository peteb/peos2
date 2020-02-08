#include "terminal.h"
#include "filesystem.h"
#include "screen.h"
#include "process.h"
#include "debug.h"

#include "support/format.h"
#include "support/pool.h"

#include "terminal_private.h"

// Declarations
static int write(int handle, const char *data, int length);
static int read(int handle, char *data, int length);
static int open(vfs_device *device, const char *path, uint32_t flags);
static void focus_terminal(uint16_t term_id);
static void create_terminal(const char *name, screen_buffer buffer);

// Global state
static p2::fixed_pool<terminal, 16> terminals;
static uint16_t current_terminal = terminals.end_sentinel();

// Definitions
void term_init()
{
  create_terminal("term0", screen_current_buffer());

  for (int i = 1; i < 5; ++i) {
    create_terminal(p2::format<10>("term%d", i).str().c_str(), screen_create_buffer());
  }
}

void create_terminal(const char *name, screen_buffer buffer)
{
  static vfs_device_driver interface =
  {
    .write = write,
    .read = read,
    .open = open,
    .close = nullptr,
    .control = nullptr,
    .seek = nullptr,
    .tell = nullptr,
    .mkdir = nullptr
  };

  uintptr_t term_id = terminals.emplace_anywhere(buffer);

  vfs_node_handle term_driver = vfs_create_node(VFS_CHAR_DEVICE);
  vfs_set_driver(term_driver, &interface, (void *)term_id);
  vfs_add_dirent(vfs_lookup("/dev/"), name, term_driver);

  if (current_terminal == terminals.end_sentinel()) {
    current_terminal = term_id;
  }

  log(term, "created terminal %s", name);
}

void term_keypress(uint16_t keycode)
{
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

static int write(int handle, const char *data, int length)
{
  return terminals[handle].syscall_write(data, length);
}

static int read(int handle, char *data, int length)
{
  return terminals[handle].syscall_read(data, length);
}

static int open(vfs_device *device, const char *path, uint32_t flags)
{
  (void)flags;
  // TODO: handle flags
  assert(path[0] == '\0');
  return (int)vfs_get_opaque(device);
}

static void focus_terminal(uint16_t term_id)
{
  if (!terminals.valid(term_id))
    return;

  terminals[term_id].focus();
  current_terminal = term_id;
}

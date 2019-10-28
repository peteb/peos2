#include "terminal.h"
#include "filesystem.h"
#include "screen.h"
#include "support/format.h"

static int write(vfs_char_device *, const char *path, const char *data, int length);
static int read(vfs_char_device *device, const char *path, char *data, int length);

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

static int write(vfs_char_device *, const char *path, const char *data, int length) {
  assert(path[0] == '\0');  // No subpaths possible on terminal
  print(data, length);
  return length;
}

static int read(vfs_char_device *device, const char *path, char *data, int length) {
  (void)device;
  (void)path;
  (void)data;
  (void)length;
  assert(path[0] == '\0');
  puts("PROMPT> ");
  data[0] = '!';
  data[1] = '\0';

  // TODO: return immediately if we can read from the consumable buffer
  // TODO: otherwise wait for enough to become available
  return 1;
}

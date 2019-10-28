#include "terminal.h"
#include "filesystem.h"
#include "screen.h"
#include "support/format.h"

static int write(vfs_char_device *, const char *path, const char *data, int length) {
  puts(p2::format<64>("dummy_write: path=%s, data=%x, length=%d") % path % (uintptr_t)data % length);
  return 0;
}

static vfs_device_driver interface = {
  .write = write
};

void term_init(const char *name) {
  vfs_node_handle term_driver = vfs_create_node(VFS_CHAR_DEVICE);
  vfs_set_driver(term_driver, &interface, nullptr);
  vfs_add_dirent(vfs_lookup("/dev/"), name, term_driver);
}

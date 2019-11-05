#include "ramfs.h"
#include "filesystem.h"
#include "screen.h"
#include "process.h"
#include "syscalls.h"

#include "support/format.h"
#include "support/pool.h"

#include "ramfs_private.h"

// Statics
static int read(int handle, char *data, int length);
static int open(vfs_device *device, const char *path, uint32_t flags);
static int control(int handle, uint32_t function, uint32_t param1, uint32_t param2);

// Global state
static p2::pool<file, 128, file_handle> files;
static p2::pool<opened_file, 32, file_handle> opened_files;

// Definitions
void ramfs_init() {
  static vfs_device_driver interface =
  {
    .write = nullptr,
    .read = read,
    .open = open,
    .control = control
  };

  vfs_node_handle mountpoint = vfs_create_node(VFS_FILESYSTEM);
  vfs_set_driver(mountpoint, &interface, nullptr);
  vfs_add_dirent(vfs_lookup("/"), "ramfs", mountpoint);
}

static int read(int handle, char *data, int length) {
  opened_file &fd = opened_files[handle];
  const file &f = files[fd.file];

  const int bytes_to_copy = p2::min(fd.position + length, f.size) - fd.position;
  const void *src = (const void *)((uintptr_t)f.start + fd.position);

  memcpy(data, src, bytes_to_copy);
  fd.position += bytes_to_copy;

  return bytes_to_copy;
}

static int open(vfs_device *, const char *path, uint32_t flags) {
  assert(path[0] == '/');
  path++;  // Step up past /

  p2::string<64> p(path);
  file_handle file_idx = files.end();

  for (size_t i = 0; i < files.size(); ++i) {
    if (files[i].name == p) {
      file_idx = i;
      break;
    }
  }

  if (file_idx == files.end()) {
    assert(flags & FLAG_OPEN_CREATE);
    // TODO: be nicer when we can't find the file
    file_idx = files.emplace_back(p, 0, 0);
    puts(p2::format<64>("ramfs: created \"%s\"", path));
  }

  return opened_files.emplace_back(file_idx, 0);
}

static int control(int handle, uint32_t function, uint32_t param1, uint32_t param2) {
  if (function == CTRL_RAMFS_SET_FILE_RANGE) {
    file &f = files[opened_files[handle].file];
    assert(f.size == 0);
    f.start = param1;
    f.size = param2;
    return 0;
  }

  return 1;
}

void ramfs_set_file_range(int fd, uintptr_t start, size_t size) {
  // TODO: check that the file has been opened with create
  proc_fd *pfd = proc_get_fd(proc_current_pid(), fd);
  assert(pfd);
  file &f = files[opened_files[pfd->value].file];
  assert(f.size == 0);
  f.start = start;
  f.size = size;
}

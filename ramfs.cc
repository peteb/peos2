#include "ramfs.h"
#include "filesystem.h"
#include "screen.h"
#include "process.h"

#include "support/format.h"
#include "support/pool.h"

static int write(int handle, const char *data, int length);
static int read(int handle, char *data, int length);
static int open(vfs_device *device, const char *path, uint32_t flags);

typedef uint16_t file_handle;

struct file {
  file(const p2::string<64> &name, uintptr_t start, size_t size)
    : name(name), start(start), size(size) {}

  p2::string<64> name;
  uintptr_t start;
  size_t size;
};

struct opened_file {
  opened_file(file_handle file, uint32_t position)
    : file(file), position(position) {}

  file_handle file;
  uint32_t position;
};

static p2::pool<file, 128, file_handle> files;
static p2::pool<opened_file, 32, file_handle> opened_files;

static vfs_device_driver interface =
{
  .write = write,
  .read = read,
  .open = open
};

void ramfs_init() {
  vfs_node_handle mountpoint = vfs_create_node(VFS_FILESYSTEM);
  vfs_set_driver(mountpoint, &interface, nullptr);
  vfs_add_dirent(vfs_lookup("/"), "ramfs", mountpoint);
}

int write(int handle, const char *data, int length) {
  (void)handle;
  (void)data;
  (void)length;
  return 0;
}

int read(int handle, char *data, int length) {
  opened_file &ofile = opened_files[handle];
  file &fil = files[ofile.file];
  int bytes_read = 0;

  // TODO: clean this up using memncpy
  char *source = (char *)(fil.start + ofile.position);
  while (ofile.position < fil.size && length > 0) {
    *data++ = *source++;
    ++ofile.position;
    --length;
    ++bytes_read;
  }

  return bytes_read;
}

int open(vfs_device *, const char *p, uint32_t flags) {
  p++; // Step up past /
  (void)flags;

  p2::string<64> path(p);
  file_handle file_idx = files.end();

  for (size_t i = 0; i < files.size(); ++i) {
    if (files[i].name == path) {
      file_idx = i;
      break;
    }
  }

  if (file_idx == files.end()) {
    // TODO: check if flags has CREATE
    // TODO: where should we put flag #defines?
    file_idx = files.emplace_back(path, 0, 0);
    puts(p2::format<64>("ramfs: created \"%s\"", p));
  }

  return opened_files.emplace_back(file_idx, 0);
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

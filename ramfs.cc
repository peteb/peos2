#include "ramfs.h"
#include "filesystem.h"
#include "process.h"
#include "syscalls.h"
#include "screen.h"
#include "debug.h"

#include "support/format.h"
#include "support/pool.h"

#include "ramfs_private.h"

// Statics
static int read(int handle, char *data, int length);
static int open(vfs_device *device, const char *path, uint32_t flags);
static int control(int handle, uint32_t function, uint32_t param1, uint32_t param2);
static int close(int handle);
static int seek(int handle, int offset, int relative);
static int tell(int handle, int *position);

// A flat list of all files and their contents
static p2::pool<file, 128, file_handle> files;

// To remember file descriptor states (position etc)
static p2::pool<opened_file, 32, file_handle> opened_files;

void ramfs_init()
{
  static vfs_device_driver interface = {
    .write = nullptr,
    .read = read,
    .open = open,
    .close = close,
    .control = control,
    .seek = seek,
    .tell = tell
  };

  vfs_node_handle mountpoint = vfs_create_node(VFS_FILESYSTEM);
  vfs_set_driver(mountpoint, &interface, nullptr);
  vfs_add_dirent(vfs_lookup("/"), "ramfs", mountpoint);
}

static int read(int handle, char *data, int length)
{
  opened_file &fd = opened_files[handle];
  const file &f = files[fd.file];

  const int bytes_to_copy = p2::min(fd.position + length, f.size) - fd.position;
  const void *src = (const void *)((uintptr_t)f.start + fd.position);

  memcpy(data, src, bytes_to_copy);
  fd.position += bytes_to_copy;
  return bytes_to_copy;
}

static int open(vfs_device *, const char *path, uint32_t flags)
{
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
    dbg_puts(ramfs, "creating file '%s'", path);
    file_idx = files.emplace_back(p, 0, 0);
  }

  return opened_files.emplace_back(file_idx, 0);
}

static int control(int handle, uint32_t function, uint32_t param1, uint32_t param2)
{
  if (function == CTRL_RAMFS_SET_FILE_RANGE) {
    file &f = files[opened_files[handle].file];
    assert(f.size == 0);
    f.start = param1;
    f.size = param2;
    return 0;
  }
  else if (function == CTRL_RAMFS_GET_FILE_RANGE) {
    file &f = files[opened_files[handle].file];
    // TODO: check that it's a RAM contiguous file
    uint32_t *start_ptr = (uint32_t *)param1;
    uint32_t *size_ptr = (uint32_t *)param2;

    if (start_ptr)
      *start_ptr = f.start;
    if (size_ptr)
      *size_ptr = f.size;
    return 0;
  }

  return 1;
}

static int close(int handle)
{
  // TODO: verify handle
  opened_files.erase(handle);
  return 0;
}

static int seek(int handle, int offset, int relative)
{
  opened_file &of = opened_files[handle];
  file &f = files[of.file];

  if (relative == SEEK_CUR) {
    of.position += offset;
  }
  else if (relative == SEEK_BEG) {
    of.position = (size_t)p2::max(offset, 0);
  }
  else {
    assert(!"invalid relative value");
  }

  of.position = p2::clamp<uint32_t>(of.position, 0u, f.size);
  // TODO: clean up overflow issues
  return 0;
}

static int tell(int handle, int *position)
{
  assert(position);
  *position = opened_files[handle].position;
  return 0;
}

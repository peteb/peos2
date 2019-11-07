#include "ramfs.h"
#include "filesystem.h"
#include "process.h"
#include "syscalls.h"
#include "screen.h"
#include "debug.h"

#include "support/format.h"
#include "support/pool.h"
#include "support/filesystem.h"

#include "ramfs_private.h"

// Statics
static int read(int handle, char *data, int length);
static int open(vfs_device *device, const char *path, uint32_t flags);
static int control(int handle, uint32_t function, uint32_t param1, uint32_t param2);
static int close(int handle);
static int seek(int handle, int offset, int relative);
static int tell(int handle, int *position);
static int mkdir(const char *path);

static p2::pool<mem_range_file, 64, file_handle> mem_range_files;
static p2::pool<dirent, 16, file_handle> directories;
static p2::pool<file_node, 64, node_handle> nodes;

static node_handle root;

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
    .tell = tell,
    .mkdir = mkdir
  };

  vfs_node_handle mountpoint = vfs_create_node(VFS_FILESYSTEM);
  vfs_set_driver(mountpoint, &interface, nullptr);
  vfs_add_dirent(vfs_lookup("/"), "ramfs", mountpoint);

  root = nodes.emplace_back(TYPE_DIRECTORY, directories.end());
}

static dirent *find_dirent(node_handle dir_node, const p2::string<16> &name)
{
  const file_node &node = nodes[dir_node];
  if (node.type != TYPE_DIRECTORY)
    return nullptr;

  uint16_t dirent_idx = node.file;

  while (dirent_idx != directories.end()) {
    dirent &entry = directories[dirent_idx];

    // TODO: compare strings without doing a copy
    if (entry.name == name)
      return &entry;

    dirent_idx = entry.next_dirent;
  }

  return nullptr;
}

static node_handle lookup(node_handle parent, const char *path)
{
  // Empty string or / references the parent
  if (!*path || (path[0] == '/' && path[1] == '\0')) {
    return parent;
  }

  assert(path[0] == '/');

  // TODO: cleanup string handling using string_view or similar
  const char *seg_start = path + 1;
  const char *next_segment = strchr(seg_start, '/');

  if (!next_segment)
    next_segment = strchr(path, '\0');

  const p2::string<16> segment(seg_start, next_segment - seg_start);

  if (const dirent *entry = find_dirent(parent, segment)) {
    return lookup(entry->node, next_segment);
  }

  return nodes.end();
}

static int read(int handle, char *data, int length)
{
  // TODO: verify that the file is in fact a mem range file
  opened_file &fd = opened_files[handle];
  file_node &node = nodes[fd.node];
  mem_range_file &file = mem_range_files[node.file];

  const int bytes_to_copy = p2::min(fd.position + length, file.size) - fd.position;
  const void *src = (const void *)((uintptr_t)file.start + fd.position);

  memcpy(data, src, bytes_to_copy);
  fd.position += bytes_to_copy;
  return bytes_to_copy;
}

static int create_file(const char *parent_path, const char *filename, uint8_t type)
{
  node_handle parent = lookup(root, parent_path);
  if (parent == nodes.end())
    return ENOENT;

  file_node &parent_node = nodes[parent];
  if (parent_node.type != TYPE_DIRECTORY)
    return ENODIR;

  // TODO: check whether the file already exists
  // TODO: check what kind of file we want to create
  file_handle file;

  if (type == TYPE_DIRECTORY) {
    file = directories.end();
  }
  else if (type == TYPE_MEM_RANGE_FILE) {
    file = mem_range_files.emplace_back(0, 0);
  }
  else {
    panic("invalid type");
  }

  node_handle file_node = nodes.emplace_back(type, file);
  file_handle dir_entry = directories.emplace_back(filename, file_node, parent_node.file);
  parent_node.file = dir_entry;

  return file_node;
}

static int open(vfs_device *, const char *path, uint32_t flags)
{
  // TODO: handle longer strings than 128 chars
  node_handle node = lookup(root, path);

  if (node == nodes.end()) {
    if (!(flags & FLAG_OPEN_CREATE))
      return ENOENT;

    p2::string<128> dir, base;
    p2::dirname(p2::string<128>(path), &dir, &base);

    int retval = create_file(dir.c_str(), base.c_str(), TYPE_MEM_RANGE_FILE);
    if (retval < 0)
      return retval;

    node = (node_handle)retval;
    dbg_puts(ramfs, "created file '%s' in '%s' as node %d", base.c_str(), dir.c_str(), node);
  }

  return opened_files.emplace_back(node, 0);
}

static int control(int handle, uint32_t function, uint32_t param1, uint32_t param2)
{
  file_node &node = nodes[opened_files[handle].node];

  if (function == CTRL_RAMFS_SET_FILE_RANGE) {
    assert(node.type == TYPE_MEM_RANGE_FILE);
    mem_range_file &file = mem_range_files[node.file];
    assert(file.size == 0);
    file.start = param1;
    file.size = param2;
    return 0;
  }
  else if (function == CTRL_RAMFS_GET_FILE_RANGE) {
    assert(node.type == TYPE_MEM_RANGE_FILE);
    mem_range_file &file = mem_range_files[node.file];
    // TODO: check that it's a RAM contiguous file
    uint32_t *start_ptr = (uint32_t *)param1;
    uint32_t *size_ptr = (uint32_t *)param2;

    if (start_ptr)
      *start_ptr = file.start;
    if (size_ptr)
      *size_ptr = file.size;
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
  // TODO: verify file type
  opened_file &fd = opened_files[handle];
  file_node &node = nodes[fd.node];
  mem_range_file &file = mem_range_files[node.file];

  if (relative == SEEK_CUR) {
    fd.position += offset;
  }
  else if (relative == SEEK_BEG) {
    fd.position = (size_t)p2::max(offset, 0);
  }
  else {
    // TODO: nicer error code
    panic("invalid relative value");
  }

  fd.position = p2::clamp<uint32_t>(fd.position, 0u, file.size);
  dbg_puts(ramfs, "seek to %d", fd.position);
  // TODO: clean up overflow issues
  return 0;
}

static int tell(int handle, int *position)
{
  assert(position);
  *position = opened_files[handle].position;
  return 0;
}

static int mkdir(const char *path)
{
  p2::string<128> dir, base;
  p2::dirname(p2::string<128>(path), &dir, &base);

  int ret = create_file(dir.c_str(), base.c_str(), TYPE_DIRECTORY);
  if (ret >= 0)
    return 0;

  return ret;
}

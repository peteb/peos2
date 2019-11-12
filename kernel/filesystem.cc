#include "filesystem.h"
#include "screen.h"
#include "assert.h"
#include "syscalls.h"
#include "process.h"
#include "debug.h"
#include "syscall_utils.h"

#include "support/pool.h"
#include "support/string.h"
#include "support/utils.h"

#include "filesystem_private.h"

// Statics
static int syscall_write(int fd, const char *data, int length);
static int syscall_read(int fd, char *data, int length);
static int syscall_open(const char *filename, uint32_t flags);
static int syscall_close(int fd);
static int syscall_control(int fd, uint32_t function, uint32_t param1, uint32_t param2);
static int syscall_seek(int fd, int offset, int relative);
static int syscall_tell(int fd, int *position);
static int syscall_mkdir(const char *path);

// Global state
static p2::pool<vfs_node, 256, vfs_node_handle> nodes;
static p2::pool<vfs_dirent, 256, decltype(vfs_node::info_node)> directories;
static p2::pool<vfs_device, 64, decltype(vfs_node::info_node)> drivers;
static p2::pool<context, 32, vfs_context> contexts;

static vfs_node_handle root_dir;

// Definitions
vfs_node_handle vfs_create_node(uint8_t type)
{
  return nodes.emplace_back(type, directories.end());
}

void vfs_add_dirent(vfs_node_handle dir_node, const char *name, vfs_node_handle node)
{
  vfs_node &parent = nodes[dir_node];
  assert(parent.type == VFS_DIRECTORY);
  parent.info_node = directories.emplace_back(name, node, parent.info_node);
}

void vfs_set_driver(vfs_node_handle driver_node, vfs_device_driver *driver, void *opaque)
{
  vfs_node &node = nodes[driver_node];
  assert(node.type & VFS_DRIVER);
  node.info_node = drivers.push_back({driver, opaque});
}

void vfs_init()
{
  root_dir = vfs_create_node(VFS_DIRECTORY);
  syscall_register(SYSCALL_NUM_WRITE, (syscall_fun)syscall_write);
  syscall_register(SYSCALL_NUM_READ, (syscall_fun)syscall_read);
  syscall_register(SYSCALL_NUM_OPEN, (syscall_fun)syscall_open);
  syscall_register(SYSCALL_NUM_CONTROL, (syscall_fun)syscall_control);
  syscall_register(SYSCALL_NUM_CLOSE, (syscall_fun)syscall_close);
  syscall_register(SYSCALL_NUM_SEEK, (syscall_fun)syscall_seek);
  syscall_register(SYSCALL_NUM_TELL, (syscall_fun)syscall_tell);
  syscall_register(SYSCALL_NUM_MKDIR, (syscall_fun)syscall_mkdir);
}

static vfs_dirent *find_dirent(vfs_node_handle dir_node, const p2::string<32> &name)
{
  const vfs_node &node = nodes[dir_node];
  assert(node.type == VFS_DIRECTORY);

  uint16_t dirent_idx = node.info_node;
  while (dirent_idx != directories.end()) {
    vfs_dirent &dirent = directories[dirent_idx];

    // TODO: compare strings without doing a copy
    if (name == p2::string<32>(dirent.name)) {
      return &dirent;
    }

    dirent_idx = dirent.next_dirent;
  }

  return nullptr;
}

static vfs_node_handle vfs_lookup_aux(vfs_node_handle parent, const char *path)
{
  // Empty string or / references the parent
  if (!*path || (path[0] == '/' && path[1] == '\0')) {
    return parent;
  }

  assert(path[0] == '/');

  // TODO: cleanup string handling using string_view or similar
  const char *seg_start = path + 1;
  const char *next_segment = strchr(seg_start, '/');
  if (!next_segment) {
    next_segment = strchr(path, '\0');
  }

  const p2::string<32> segment(seg_start, next_segment - seg_start);
  if (const vfs_dirent *dirent = find_dirent(parent, segment)) {
    return vfs_lookup_aux(dirent->node, next_segment);
  }

  return nodes.end();
}

vfs_node_handle vfs_lookup(const char *path)
{
  // TODO: a better way to return "path not found" than nodes.end()
  return vfs_lookup_aux(root_dir, path);
}

static void vfs_print_aux(vfs_node_handle parent, int level)
{
  p2::string<64> prefix;
  for (int i = 0; i < level; ++i) {
    prefix.append("  ");
  }

  const vfs_node &node = nodes[parent];
  print("  ");
  puts(p2::format<64>("(type: %x)", node.type));

  if (node.type == VFS_DIRECTORY) {
    uint16_t dirent_idx = node.info_node;
    while (dirent_idx != directories.end()) {
      const vfs_dirent &dirent = directories[dirent_idx];
      print(prefix);
      print(dirent.name);
      vfs_print_aux(dirent.node, level + 1);
      dirent_idx = dirent.next_dirent;
    }
  }
}

void vfs_print()
{
  print("root");
  vfs_print_aux(root_dir, 1);
}

struct ffd_retval {
  ffd_retval(vfs_node_handle node_idx, const char *rest_path) : node_idx(node_idx), rest_path(rest_path) {}
  vfs_node_handle node_idx;
  const char *rest_path;
};

static p2::opt<ffd_retval> find_first_driver(vfs_node_handle parent_idx, const char *path)
{
  if (nodes[parent_idx].type & VFS_DRIVER) {
    return {parent_idx, path};
  }

  // Empty string or / references the parent
  if (!*path || (path[0] == '/' && path[1] == '\0')) {
    return {nodes.end(), path};
  }

  assert(path[0] == '/');

  // TODO: cleanup string handling using string_view or similar
  const char *seg_start = path + 1;
  const char *next_segment = strchr(seg_start, '/');

  if (!next_segment) {
    next_segment = strchr(path, '\0');
  }

  const p2::string<32> segment(seg_start, next_segment - seg_start);
  if (const vfs_dirent *dirent = find_dirent(parent_idx, segment)) {
    return find_first_driver(dirent->node, next_segment);
  }

  return {};
}

void *vfs_get_opaque(vfs_device *device)
{
  return device->opaque;
}

int vfs_close(vfs_context context_handle, vfs_fd local_fd)
{
  context &context_ = contexts[context_handle];
  filedesc &fd_info = context_.descriptors[local_fd];

  assert(fd_info.device && fd_info.device->driver);

  dbg_puts(vfs, "closing %d.%d", context_handle, local_fd);
  context_.descriptors.erase(local_fd);

  if (!fd_info.device->driver->close)
    return ENOSUPPORT;

  // TODO: what do we do if the driver fails to close the file? But
  // we've already removed our local mapping?
  return fd_info.device->driver->close(fd_info.device_local_handle);
}

static int syscall_write(int fd, const char *data, int length)
{
  verify_ptr(vfs, data);

  vfs_context context_handle = proc_get_file_context(*proc_current_pid());

  context &context_ = contexts[context_handle];
  filedesc &fd_info = context_.descriptors[fd];
  assert(fd_info.device && fd_info.device->driver);

  if (!fd_info.device->driver->write)
    return ENOSUPPORT;

  return fd_info.device->driver->write(fd_info.device_local_handle, data, length);
}

static int syscall_read(int fd, char *data, int length)
{
  verify_ptr(vfs, data);
  // TODO: verify that the range data, data + length is valid

  p2::res<size_t> read_result = vfs_read(proc_get_file_context(*proc_current_pid()), fd, data, length);

  if (!read_result) {
    return read_result.error();
  }
  else {
    // TODO: handle possible overflow
    return *read_result;
  }
}

p2::res<size_t> vfs_read(vfs_context context_handle, vfs_fd fd, char *data, int length)
{
  context &context_ = contexts[context_handle];
  filedesc &fd_info = context_.descriptors[fd];
  assert(fd_info.device && fd_info.device->driver);

  if (!fd_info.device->driver->read)
    return p2::failure(ENOSUPPORT);

  int retval = fd_info.device->driver->read(fd_info.device_local_handle, data, length);

  if (retval < 0)
    return p2::failure(retval);

  return p2::success((size_t)retval);
}

p2::res<vfs_fd> vfs_open(vfs_context context_handle, const char *filename, uint32_t flags)
{
  context &context_ = contexts[context_handle];

  auto driver = find_first_driver(root_dir, filename);
  if (!driver)
    return p2::failure(ENODRIVER);

  const vfs_node &driver_node = nodes[driver->node_idx];
  assert(driver_node.type & VFS_DRIVER);
  assert(driver_node.info_node != drivers.end());

  vfs_device &device_node = drivers[driver_node.info_node];
  assert(device_node.driver);

  if (!device_node.driver->open)
    return p2::failure(ENOSUPPORT);

  int device_local_handle = device_node.driver->open(&device_node, driver->rest_path, flags);

  if (device_local_handle < 0)
    return p2::failure(device_local_handle);

  vfs_fd fd = context_.descriptors.emplace_back(&device_node, device_local_handle, flags);
  dbg_puts(vfs, "opened %s at %d.%d", filename, context_handle, fd);
  return p2::success(fd);
}

int vfs_seek(vfs_context context_handle, vfs_fd fd, int offset, int relative)
{
  context &context_ = contexts[context_handle];
  filedesc &fd_info = context_.descriptors[fd];
  assert(fd_info.device);

  if (!fd_info.device->driver->seek)
    return ENOSUPPORT;

  return fd_info.device->driver->seek(fd_info.device_local_handle, offset, relative);
}

p2::res<vfs_context> vfs_create_context()
{
  if (contexts.full())
    return p2::failure(ENOSPACE);

  return p2::success(contexts.emplace_back());
}

void vfs_destroy_context(vfs_context context_handle)
{
  dbg_puts(vfs, "destroying context %d...", context_handle);

  context &context_ = contexts[context_handle];

  for (int i = 0; i < context_.descriptors.watermark(); ++i) {
    if (context_.descriptors.valid(i))
      vfs_close(context_handle, i);
  }
}

void vfs_close_not_matching(vfs_context context_handle, uint32_t flags)
{
  context &context_ = contexts[context_handle];

  for (int i = 0; i < context_.descriptors.watermark(); ++i) {
    if (context_.descriptors.valid(i) && !(context_.descriptors[i].flags & flags))
      vfs_close(context_handle, i);
  }
}

static int syscall_open(const char *filename, uint32_t flags)
{
  verify_ptr(vfs, filename);
  // TODO: verify that the whole string is in valid memory

  p2::res<vfs_fd> open_res = vfs_open(proc_get_file_context(*proc_current_pid()), filename, flags);

  if (open_res) {
    assert(*open_res >= 0);
    return *open_res;
  }
  else {
    assert(open_res.error() < 0);
    return open_res.error();
  }
}

static int syscall_mkdir(const char *path)
{
  verify_ptr(vfs, path);

  auto driver = find_first_driver(root_dir, path);
  if (!driver)
    return ENODRIVER;

  const vfs_node &driver_node = nodes[driver->node_idx];
  assert(driver_node.type & VFS_DRIVER);
  assert(driver_node.info_node != drivers.end());

  vfs_device &device_node = drivers[driver_node.info_node];
  assert(device_node.driver);

  if (!device_node.driver->mkdir)
    return ENOSUPPORT;

  return device_node.driver->mkdir(driver->rest_path);
}

static int syscall_close(int fd)
{
  return vfs_close(proc_get_file_context(*proc_current_pid()), fd);
}

static int syscall_seek(int fd, int offset, int relative)
{
  return vfs_seek(proc_get_file_context(*proc_current_pid()), fd, offset, relative);
}

static int syscall_tell(int fd, int *position)
{
  assert(position);
  verify_ptr(vfs, position);

  vfs_context context_handle = proc_get_file_context(*proc_current_pid());
  context &context_ = contexts[context_handle];
  filedesc &fd_info = context_.descriptors[fd];
  assert(fd_info.device && fd_info.device->driver);

  if (!fd_info.device->driver->tell)
    return ENOSUPPORT;

  return fd_info.device->driver->tell(fd_info.device_local_handle, position);
}

static int syscall_control(int fd, uint32_t function, uint32_t param1, uint32_t param2)
{
  vfs_context context_handle = proc_get_file_context(*proc_current_pid());
  context &context_ = contexts[context_handle];
  filedesc &fd_info = context_.descriptors[fd];
  assert(fd_info.device && fd_info.device->driver);

  if (!fd_info.device->driver->control)
    return ENOSUPPORT;

  return fd_info.device->driver->control(fd_info.device_local_handle, function, param1, param2);
}

#include "filesystem.h"
#include "screen.h"
#include "syscalls.h"
#include "process.h"
#include "debug.h"
#include "syscall_utils.h"

#include "support/pool.h"
#include "support/string.h"
#include "support/utils.h"
#include "support/assert.h"

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
static int syscall_dup2(int fd, int alias_fd);

static int read_locally(int handle, char *data, int length);
static int open_locally(vfs_device *device, const char *path, uint32_t flags);
static int close_locally(int handle);

// Global state
static p2::pool<vfs_node, 256, vfs_node_handle> nodes;
static p2::pool<vfs_dirent, 256, decltype(vfs_node::info_node)> directories;
static p2::pool<vfs_device, 64, decltype(vfs_node::info_node)> drivers;
static vfs_node_handle root_dir;

static p2::pool<context, 64, vfs_context> contexts;
static p2::pool<opened_file, 64, opened_file_handle> opened_files;
static p2::pool<locally_opened_file, 64, opened_file_handle> locally_opened_files;

static vfs_node_handle local_driver_handle;

// Definitions
vfs_node_handle vfs_create_node(uint8_t type)
{
  return nodes.emplace_anywhere(type, directories.end_sentinel());
}

void vfs_add_dirent(vfs_node_handle dir_node, const char *name, vfs_node_handle node)
{
  vfs_node &parent = nodes[dir_node];
  assert(parent.type == VFS_DIRECTORY);
  parent.info_node = directories.emplace_anywhere(name, node, parent.info_node);
}

void vfs_set_driver(vfs_node_handle driver_node, vfs_device_driver *driver, void *opaque)
{
  vfs_node &node = nodes[driver_node];
  assert(node.type & VFS_DRIVER);
  node.info_node = drivers.emplace_anywhere(driver, opaque);
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
  syscall_register(SYSCALL_NUM_DUP2, (syscall_fun)syscall_dup2);

  // Setup the VFS driver so it's easy to manipulate and read the VFS
  // TODO: set this on the root node instead, and change "find_first_driver" to "find_deepest_driver"
  static vfs_device_driver local_driver = {
    .write = nullptr,
    .read = read_locally,
    .open = open_locally,
    .close = close_locally,
    .control = nullptr,
    .seek = nullptr,
    .tell = nullptr,
    .mkdir = nullptr
  };

  local_driver_handle = vfs_create_node(VFS_FILESYSTEM);
  vfs_set_driver(local_driver_handle, &local_driver, nullptr);
}

static vfs_dirent *find_dirent(vfs_node_handle dir_node, const p2::string<32> &name)
{
  const vfs_node &node = nodes[dir_node];
  assert(node.type == VFS_DIRECTORY);

  uint16_t dirent_idx = node.info_node;
  while (dirent_idx != directories.end_sentinel()) {
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
  // TODO: make this non-recursive to save on kernel stack space

  // Empty string or / references the parent
  if (!*path || (path[0] == '/' && path[1] == '\0')) {
    return parent;
  }

  if (path[0] != '/')
    return nodes.end_sentinel();

  // TODO: cleanup string handling using string_view or similar
  const char *seg_start = path + 1;
  const char *next_segment = strchr((char *)seg_start, '/');
  if (!next_segment) {
    next_segment = strchr((char *)path, '\0');
  }

  const p2::string<32> segment(seg_start, next_segment - seg_start);
  if (const vfs_dirent *dirent = find_dirent(parent, segment)) {
    return vfs_lookup_aux(dirent->node, next_segment);
  }

  return nodes.end_sentinel();
}

vfs_node_handle vfs_lookup(const char *path)
{
  // TODO: a better way to return "path not found" than nodes.end_sentinel()
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
    while (dirent_idx != directories.end_sentinel()) {
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

static p2::opt<ffd_retval> find_first_driver_aux(vfs_node_handle parent_idx, const char *path)
{
  // TODO: make this non-recursive to save on stack space

  if (nodes[parent_idx].type & VFS_DRIVER) {
    return {parent_idx, path};
  }

  // Empty string or / references the parent
  if (!*path || (path[0] == '/' && path[1] == '\0') || path[0] != '/') {
    return {};
  }

  // TODO: cleanup string handling using string_view or similar
  const char *seg_start = path + 1;
  const char *next_segment = strchr((char *)seg_start, '/');

  if (!next_segment) {
    next_segment = strchr((char *)path, '\0');
  }

  const p2::string<32> segment(seg_start, next_segment - seg_start);
  if (const vfs_dirent *dirent = find_dirent(parent_idx, segment)) {
    return find_first_driver_aux(dirent->node, next_segment);
  }

  // Fall back to the local driver for manipulating and reading the VFS
  return {};
}

static p2::opt<ffd_retval> find_first_driver(vfs_node_handle parent_idx, const char *path)
{
  if (auto ret = find_first_driver_aux(parent_idx, path)) {
    return ret;
  }
  else {
    return {local_driver_handle, path};
  }
}

void *vfs_get_opaque(vfs_device *device)
{
  return device->opaque;
}

int vfs_close(vfs_context context_handle, vfs_fd local_fd)
{
  context &context_ = contexts[context_handle];
  opened_file_handle file_handle = context_.descriptors[local_fd];
  opened_file &file_info = opened_files[file_handle];

  assert(file_info.device && file_info.device->driver);
  context_.descriptors.erase(local_fd);

  if (--file_info.ref_count == 0) {
    dbg_puts(vfs, "closing global opened file %d", file_handle);

    // TODO: what do we do if the driver fails to close the file? But
    // we've already removed our local mapping?
    int retval = 0;

    if (file_info.device->driver->close)
      retval = file_info.device->driver->close(file_info.device_local_handle);

    opened_files.erase(file_handle);
    return retval;
  }

  return 0;
}

static p2::res<opened_file *> fetch_opened_file(vfs_context context_handle, vfs_fd fd)
{
  if (!contexts.valid(context_handle))
    return p2::failure(ENOENT);

  context &context_ = contexts[context_handle];

  if (!context_.descriptors.valid(fd))
    return p2::failure(ENOENT);

  opened_file_handle file_handle = context_.descriptors[fd];

  if (!opened_files.valid(file_handle))
    return p2::failure(ENOENT);

  opened_file &file = opened_files[file_handle];

  if (!file.device || !file.device->driver)
    return p2::failure(EINCONSTATE);

  return p2::success(&file);
}

static int syscall_write(int fd, const char *data, int length)
{
  verify_ptr(vfs, data);

  p2::res<opened_file *> file = fetch_opened_file(proc_get_file_context(*proc_current_pid()), fd);
  if (!file)
    return file.error();

  if (!(*file)->device->driver->write)
    return ENOSUPPORT;

  return (*file)->device->driver->write((*file)->device_local_handle, data, length);
}

static int syscall_read(int fd, char *data, int length)
{
  verify_buf(vfs, data, length);

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
  p2::res<opened_file *> file = fetch_opened_file(context_handle, fd);

  if (!file) {
    return p2::failure(file.error());
  }

  if (!(*file)->device->driver->read)
    return p2::failure(ENOSUPPORT);

  int retval = (*file)->device->driver->read((*file)->device_local_handle, data, length);

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
  assert(driver_node.info_node != drivers.end_sentinel());

  vfs_device &device_node = drivers[driver_node.info_node];
  assert(device_node.driver);

  if (!device_node.driver->open)
    return p2::failure(ENOSUPPORT);

  int device_local_handle = device_node.driver->open(&device_node, driver->rest_path, flags);

  if (device_local_handle < 0)
    return p2::failure(device_local_handle);

  opened_file_handle ofh = opened_files.emplace_anywhere(&device_node, device_local_handle, flags);
  vfs_fd fd = context_.descriptors.emplace_anywhere(ofh);
  dbg_puts(vfs, "opened %s at %d.%d", filename, context_handle, fd);
  return p2::success(fd);
}

int vfs_seek(vfs_context context_handle, vfs_fd fd, int offset, int relative)
{
  p2::res<opened_file *> file = fetch_opened_file(context_handle, fd);

  if (!file)
    return file.error();

  if (!(*file)->device->driver->seek)
    return ENOSUPPORT;

  return (*file)->device->driver->seek((*file)->device_local_handle, offset, relative);
}

p2::res<vfs_context> vfs_create_context()
{
  if (contexts.full())
    return p2::failure(ENOSPACE);
  return p2::success(contexts.emplace_anywhere());
}

void vfs_destroy_context(vfs_context context_handle)
{
  dbg_puts(vfs, "destroying context %d...", context_handle);

  context &context_ = contexts[context_handle];

  for (int i = 0; i < context_.descriptors.watermark(); ++i) {
    if (context_.descriptors.valid(i))
      vfs_close(context_handle, i);
  }

  contexts.erase(context_handle);
}

void vfs_close_not_matching(vfs_context context_handle, uint32_t flags)
{
  context &context_ = contexts[context_handle];

  for (int i = 0; i < context_.descriptors.watermark(); ++i) {
    if (!context_.descriptors.valid(i))
      continue;

    opened_file &file = opened_files[context_.descriptors[i]];
    if (!(file.flags & flags))
      vfs_close(context_handle, i);
  }
}

p2::res<vfs_context> vfs_fork_context(vfs_context context_handle)
{
  auto new_context = vfs_create_context();
  if (!new_context)
    return p2::failure(new_context.error());

  context &source_context = contexts[context_handle];

  for (int i = 0; i < source_context.descriptors.watermark(); ++i) {
    if (source_context.descriptors.valid(i)) {
      vfs_alias_fd(context_handle, i, *new_context, i);
    }
  }

  return p2::success(*new_context);
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
  assert(driver_node.info_node != drivers.end_sentinel());

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

  p2::res<opened_file *> file = fetch_opened_file(proc_get_file_context(*proc_current_pid()), fd);

  if (!file)
    return file.error();

  if (!(*file)->device->driver->tell)
    return ENOSUPPORT;

  return (*file)->device->driver->tell((*file)->device_local_handle, position);
}

static int syscall_control(int fd, uint32_t function, uint32_t param1, uint32_t param2)
{
  p2::res<opened_file *> file = fetch_opened_file(proc_get_file_context(*proc_current_pid()), fd);

  if (!file)
    return file.error();

  if (!(*file)->device->driver->control)
    return ENOSUPPORT;

  return (*file)->device->driver->control((*file)->device_local_handle, function, param1, param2);
}

int vfs_alias_fd(vfs_context src_ctx_handle, vfs_fd src_fd, vfs_context dst_ctx_handle, vfs_fd dst_fd)
{
  context &source_context = contexts[src_ctx_handle];
  context &dest_context = contexts[dst_ctx_handle];

  dbg_puts(vfs, "dup2: src_fd=%d, dest_fd=%d, src_ctx=%d, dest_ctx=%d", src_fd, dst_fd, src_ctx_handle, dst_ctx_handle);

  if (!source_context.descriptors.valid(src_fd))
    return ENOENT;

  if (dest_context.descriptors.valid(dst_fd) &&
      dest_context.descriptors[dst_fd] == source_context.descriptors[src_fd])
    return 0;

  if (dest_context.descriptors.valid(dst_fd)) {
    // If the alias fd already exists, close it
    vfs_close(dst_ctx_handle, dst_fd);
  }

  dest_context.descriptors.emplace(dst_fd, source_context.descriptors[src_fd]);
  ++opened_files[source_context.descriptors[src_fd]].ref_count;
  return 0;
}

static int syscall_dup2(int fd, int alias_fd)
{
  vfs_context current_context = proc_get_file_context(*proc_current_pid());
  return vfs_alias_fd(current_context, fd, current_context, alias_fd);
}

static int read_locally(int handle, char *data, int length)
{
  locally_opened_file &opened_file = locally_opened_files[handle];

  vfs_node &node_ = nodes[opened_file.node];
  assert(node_.type & VFS_DIRECTORY);

  if (node_.info_node == directories.end_sentinel()) {
    // No entries to read
    return 0;
  }

  uint16_t dirent_idx = node_.info_node;
  int byte_offset = 0;
  int bytes_written = 0;

  while (dirent_idx != directories.end_sentinel() && length > 0) {
    vfs_dirent &entry = directories[dirent_idx];

    if (opened_file.position >= byte_offset &&
        opened_file.position < byte_offset + (int)sizeof(dirent_t)) {

      // Add this data
      int block_offset = opened_file.position % sizeof(dirent_t);
      int copy_length = p2::min((int)sizeof(dirent_t) - block_offset, length);

      dirent_t block;
      memcpy(block.name, entry.name.c_str(), p2::min(entry.name.size() + 1, (int)sizeof(block.name)));
      memcpy(data, (char *)&block + block_offset, copy_length);

      data += copy_length;
      opened_file.position += copy_length;
      length -= copy_length;
      bytes_written += copy_length;
    }

    byte_offset += sizeof(dirent_t);
    dirent_idx = entry.next_dirent;
  }

  return bytes_written;
}

static int open_locally(vfs_device */*device*/, const char *path, uint32_t /*flags*/)
{
  // TODO: rename these handles according to style, and use p2::opt
  vfs_node_handle dir_handle = vfs_lookup(path);
  if (dir_handle == nodes.end_sentinel())
    return ENOENT;

  dbg_puts(vfs, "opened %s (node %d)", path, dir_handle);

  return locally_opened_files.emplace_anywhere(dir_handle);
}

static int close_locally(int handle)
{
  // TODO: verify
  locally_opened_files.erase(handle);
  return 0;
}

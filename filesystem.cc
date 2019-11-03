#include "filesystem.h"
#include "support/pool.h"
#include "support/string.h"
#include "support/utils.h"
#include "screen.h"
#include "assert.h"
#include "syscalls.h"
#include "process.h"

struct vfs_node {
  vfs_node(uint8_t type, vfs_node_handle info_node)
    : type(type),
      info_node(info_node)
  {}

  uint8_t type;
  // TODO: permissions, owner, etc.
  vfs_node_handle info_node;  // Index into type-specific block
};

struct vfs_dirent {
  vfs_dirent(const char *name, vfs_node_handle node, uint16_t next_dirent)
    : name(name),
      node(node),
      next_dirent(next_dirent)
  {}

  p2::string<32> name;
  vfs_node_handle node;
  uint16_t next_dirent;
};

struct vfs_device {
  vfs_device(vfs_device_driver *driver, void *opaque)
    : driver(driver),
      opaque(opaque)
  {}

  vfs_device_driver *driver;
  void *opaque;
};

static uint32_t syscall_write(int fd, const char *data, int length);
static uint32_t syscall_read(int fd, char *data, int length);
static int syscall_open(const char *filename, uint32_t flags);

static p2::pool<vfs_node, 256, vfs_node_handle> nodes;
static p2::pool<vfs_dirent, 256, decltype(vfs_node::info_node)> directories;
static p2::pool<vfs_device, 64, decltype(vfs_node::info_node)> drivers;
static vfs_node_handle root_dir;

vfs_node_handle vfs_create_node(uint8_t type) {
  return nodes.emplace_back(type, directories.end());
}

void vfs_add_dirent(vfs_node_handle dir_node, const char *name, vfs_node_handle node) {
  vfs_node &parent = nodes[dir_node];
  assert(parent.type == VFS_DIRECTORY);
  parent.info_node = directories.emplace_back(name, node, parent.info_node);
}

void vfs_set_driver(vfs_node_handle driver_node, vfs_device_driver *driver, void *opaque) {
  vfs_node &node = nodes[driver_node];
  assert(node.type & VFS_DRIVER);
  node.info_node = drivers.push_back({driver, opaque});
}

void vfs_init() {
  root_dir = vfs_create_node(VFS_DIRECTORY);
  syscall_register(SYSCALL_NUM_WRITE, (syscall_fun)syscall_write);
  syscall_register(SYSCALL_NUM_READ, (syscall_fun)syscall_read);
  syscall_register(SYSCALL_NUM_OPEN, (syscall_fun)syscall_open);
}

static vfs_dirent *find_dirent(vfs_node_handle dir_node, const p2::string<32> &name) {
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

static vfs_node_handle vfs_lookup_aux(vfs_node_handle parent, const char *path) {
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

vfs_node_handle vfs_lookup(const char *path) {
  // TODO: a better way to return "path not found" than nodes.end()
  return vfs_lookup_aux(root_dir, path);
}

static void vfs_print_aux(vfs_node_handle parent, int level) {
  p2::string<64> prefix;
  for (int i = 0; i < level; ++i) {
    prefix.append("  ");
  }

  const vfs_node &node = nodes[parent];
  print("  ");
  puts(p2::format<64>("(type: %x)") % node.type);

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

void vfs_print() {
  print("root");
  vfs_print_aux(root_dir, 1);
}

struct ffd_retval {
  vfs_node_handle node_idx;
  const char *rest_path;
};

static ffd_retval find_first_driver(vfs_node_handle parent_idx, const char *path) {
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

  return {nodes.end(), next_segment};
}

void *vfs_get_opaque(vfs_device *device) {
  return device->opaque;
}

static uint32_t syscall_write(int fd, const char *data, int length) {
  proc_fd *pfd = proc_get_fd(proc_current_pid(), fd);
  assert(pfd);

  vfs_device *device_node = (vfs_device *)pfd->opaque;
  assert(device_node && device_node->driver && device_node->driver->write);

  return device_node->driver->write(pfd->value, data, length);
}

static uint32_t syscall_read(int fd, char *data, int length) {
  proc_fd *pfd = proc_get_fd(proc_current_pid(), fd);
  assert(pfd);

  vfs_device *device_node = (vfs_device *)pfd->opaque;
  assert(device_node && device_node->driver && device_node->driver->read);

  return device_node->driver->read(pfd->value, data, length);
}

static int syscall_open(const char *filename, uint32_t flags) {
  auto driver = find_first_driver(root_dir, filename);
  assert(driver.node_idx != nodes.end());

  const vfs_node &driver_node = nodes[driver.node_idx];
  assert(driver_node.type & VFS_DRIVER);
  assert(driver_node.info_node != drivers.end());

  vfs_device &device_node = drivers[driver_node.info_node];
  assert(device_node.driver);
  assert(device_node.driver->open);
  // TODO: handle the error cases better than asserts

  int retval = device_node.driver->open(&device_node, driver.rest_path, flags);
  // TODO: check that it returned OK
  return proc_create_fd(proc_current_pid(), {(void *)&device_node, retval});
}

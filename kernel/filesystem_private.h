// -*- c++ -*-

#ifndef PEOS2_FILESYSTEM_PRIVATE_H
#define PEOS2_FILESYSTEM_PRIVATE_H

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

struct filedesc {
  filedesc(vfs_device *device, int handle, uint32_t flags)
    : device(device), device_local_handle(handle), flags(flags) {}

  vfs_device *device;
  int         device_local_handle;
  uint32_t    flags;
};

struct context {
  p2::pool<filedesc, 16, vfs_fd> descriptors;
};

#endif // !PEOS2_FILESYSTEM_PRIVATE_H

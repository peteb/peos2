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


#endif // !PEOS2_FILESYSTEM_PRIVATE_H

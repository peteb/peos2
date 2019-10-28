// -*- c++ -*-

#ifndef PEOS2_FILESYSTEM_H
#define PEOS2_FILESYSTEM_H

#include <stdint.h>
#include "support/string.h"

#define VFS_DIRECTORY    0x01
#define VFS_CHAR_DEVICE  0x02

typedef uint16_t vfs_node_handle;

struct vfs_char_device;

struct vfs_device_driver {
  int (*write)(vfs_char_device *device, const char *path, const char *data, int length);
  int (*read)(vfs_char_device *device, const char *path, char *data, int length);
};

struct vfs_node {
  uint8_t type;
  // TODO: permissions, owner, etc.
  uint16_t info_node;  // Index into type-specific block
};

struct vfs_dirent {
  const char *name;
  vfs_node_handle node;
  uint16_t next_dirent;
};

struct vfs_char_device {
  vfs_device_driver *driver;
  void *opaque;
};

// Kernel/driver functions
void vfs_init();
void vfs_print();

vfs_node_handle vfs_create_node(uint8_t type);
void vfs_add_dirent(vfs_node_handle dir_node, const char *name, vfs_node_handle node);
void vfs_set_driver(vfs_node_handle dev_node, vfs_device_driver *driver, void *opaque);
vfs_node_handle vfs_lookup(const char *path);

#endif // !PEOS2_FILESYSTEM_H

// -*- c++ -*-

#ifndef PEOS2_FILESYSTEM_H
#define PEOS2_FILESYSTEM_H

#include <stdint.h>

#define VFS_DIRECTORY    0x01
#define VFS_DRIVER       0x02
#define VFS_CHAR_DEVICE  (0x10|VFS_DRIVER)
#define VFS_FILESYSTEM   (0x20|VFS_DRIVER|VFS_DIRECTORY)

// Types
typedef uint16_t vfs_node_handle;
struct vfs_device;

struct vfs_device_driver {
  int (*write)(int handle, const char *data, int length);
  int (*read)(int handle, char *data, int length);
  int (*open)(vfs_device *device, const char *path, uint32_t flags);
  int (*control)(int handle, uint32_t function, uint32_t param1, uint32_t param2);
};

// Kernel/driver functions
void            vfs_init();
void            vfs_print();
vfs_node_handle vfs_create_node(uint8_t type);
void            vfs_add_dirent(vfs_node_handle dir_node, const char *name, vfs_node_handle node);
void            vfs_set_driver(vfs_node_handle dev_node, vfs_device_driver *driver, void *opaque);
vfs_node_handle vfs_lookup(const char *path);
void            *vfs_get_opaque(vfs_device *device);

#endif // !PEOS2_FILESYSTEM_H

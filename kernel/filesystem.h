// -*- c++ -*-

#ifndef PEOS2_FILESYSTEM_H
#define PEOS2_FILESYSTEM_H

#include <stdint.h>
#include <stddef.h>

#include "support/result.h"

#define VFS_DIRECTORY    0x01
#define VFS_DRIVER       0x02
#define VFS_CHAR_DEVICE  (0x10|VFS_DRIVER)
#define VFS_FILESYSTEM   (0x20|VFS_DRIVER|VFS_DIRECTORY)

// Types
typedef uint16_t vfs_node_handle;
typedef uint16_t vfs_context;
typedef uint16_t vfs_fd;

struct vfs_device;

struct vfs_device_driver {
  //
  // write - writes to a file pointed to by the handle
  // @handle: a handle local to the filesystem or driver
  // @data: data to write
  // @length: length of data in number of bytes
  //
  // Writes to a file at the current position (if applicable). Memory
  // [data, data + length] needs to be valid for the process.
  //
  // Returns the number of bytes written (which can be less than
  // `length`). A negative value represents error.
  //
  int (*write)(int handle, const char *data, int length);

  //
  // read - reads up to `length` number of bytes
  // @handle: filesystem local
  // @data: valid buffer for the process
  // @length: maximum number of bytes to read
  //
  // Reads up to `length` number of bytes. Specific behavior is up to
  // the filesystem driver, but generally 0 is only returned if
  // `length` is 0 or if the underlying stream has reached its end.
  //
  // If the handle represents a directory, reads will return dirent
  // binary data. Effects of concurrent modifications to the
  // filesystem are undefined except that the output should be
  // well-formed with respect to binary layout. There's no guarantee
  // that a following `open` using a dirent will work.
  //
  // Returns the number of bytes read (which can be less than
  // `length`, but greater than zero). A negative value represents
  // error.
  //
  int (*read)(int handle, char *data, int length);

  //
  // open - creates a local filesystem handle
  // @device: the device added using `vfs_set_driver`
  // @path: filepath excluding mountpoint
  // @flags: flags given in syscalls_decls.h, FLAG_OPEN_*
  //
  // Creates a filesystem/driver local file handle to represent the
  // opened file.
  //
  // Returns the file handle id or a negative value. on error.
  //
  int (*open)(vfs_device *device, const char *path, uint32_t flags);

  int (*close)(int handle);

  //
  // control - executes driver-specific functions
  // @handle: file handle to manipulate
  // @function: CTRL_* values in syscall_defs.h
  // @paramN: depends on the function
  //
  // Executes, gets or sets data. Parameters have different meaning
  // depending on `function`.
  //
  // Returns a negative value on error.
  //
  int (*control)(int handle, uint32_t function, uint32_t param1, uint32_t param2);

  //
  // seek - updates the read position for the given handle
  // @handle: which handle to update
  // @offset: update position with this offset
  // @relative: what the base is. SEEK_CUR or SEEK_BEG
  //
  int (*seek)(int handle, int offset, int relative);

  int (*tell)(int handle, int *position);

  int (*mkdir)(const char *path);
};

// Filesystem management; creating nodes, registering drivers, etc.
void            vfs_init();
void            vfs_print();
vfs_node_handle vfs_create_node(uint8_t type);
void            vfs_add_dirent(vfs_node_handle dir_node, const char *name, vfs_node_handle node);
void            vfs_set_driver(vfs_node_handle dev_node, vfs_device_driver *driver, void *opaque);
vfs_node_handle vfs_lookup(const char *path);
void            *vfs_get_opaque(vfs_device *device);

// Syscall-like functions but for the kernel
p2::res<vfs_fd> vfs_open(vfs_context context_handle, const char *filename, uint32_t flags);
p2::res<size_t> vfs_read(vfs_context context_handle, vfs_fd fd, char *data, int length);
int             vfs_seek(vfs_context context_handle, vfs_fd fd, int offset, int relative);
int             vfs_close(vfs_context context_handle, vfs_fd fd);
void            vfs_close_not_matching(vfs_context context_handle, uint32_t flags);
int             vfs_alias_fd(vfs_context src_ctx_handle, vfs_fd src_fd, vfs_context dst_ctx_handle, vfs_fd dst_fd);

// Managing contexts
p2::res<vfs_context> vfs_create_context();
void                 vfs_destroy_context(vfs_context context_handle);
p2::res<vfs_context> vfs_fork_context(vfs_context context_handle);

#endif // !PEOS2_FILESYSTEM_H

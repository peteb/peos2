// -*- c++ -*-
//
// A basic RAM-based filesystem without deletion support. Used for
// initial setup. Files and directories are created over the VFS
// interface, but supports special files that reference absolute
// memory.
//

#ifndef PEOS2_RAMFS_H
#define PEOS2_RAMFS_H

#include <stdint.h>
#include <stddef.h>

void ramfs_init();
void ramfs_set_file_range(int fd, uintptr_t start, size_t size);

#endif // !PEOS2_RAMFS_H

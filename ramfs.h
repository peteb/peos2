// -*- c++ -*-
//
// A basic RAM-based filesystem without deletion support. Used for
// initial setup. Files and directories are created over the VFS
// interface, but supports special files that reference absolute
// memory.
//

#ifndef PEOS2_RAMFS_H
#define PEOS2_RAMFS_H

void ramfs_init();

#endif // !PEOS2_RAMFS_H

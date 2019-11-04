// -*- c++ -*-

#ifndef PEOS2_RAMFS_PRIVATE_H
#define PEOS2_RAMFS_PRIVATE_H

typedef uint16_t file_handle;

struct file {
  file(const p2::string<64> &name, uintptr_t start, size_t size)
    : name(name), start(start), size(size) {}

  p2::string<64> name;
  uintptr_t start;
  size_t size;
};

struct opened_file {
  opened_file(file_handle file, uint32_t position)
    : file(file), position(position) {}

  file_handle file;
  uint32_t position;
};

#endif // !PEOS2_RAMFS_PRIVATE_H

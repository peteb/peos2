// -*- c++ -*-

#ifndef PEOS2_RAMFS_PRIVATE_H
#define PEOS2_RAMFS_PRIVATE_H

#define TYPE_MEM_RANGE_FILE  1
#define TYPE_DIRECTORY       2

typedef uint16_t file_handle;
typedef uint16_t node_handle;

struct opened_file {
  opened_file(node_handle node, uint32_t position) : node(node), position(position) {}

  node_handle node;
  uint32_t position;
};

struct mem_range_file {
  mem_range_file(uintptr_t start, size_t size) : start(start), size(size) {}

  uintptr_t start;
  size_t size;
};

struct dirent {
  dirent(const char *name, node_handle node, file_handle next_dirent)
    : name(name), node(node), next_dirent(next_dirent)
  {}

  p2::string<16> name;
  node_handle node;
  file_handle next_dirent;
};

struct file_node {
  file_node(uint8_t type, file_handle file) : type(type), file(file) {}

  uint8_t type;
  file_handle file;
};

#endif // !PEOS2_RAMFS_PRIVATE_H

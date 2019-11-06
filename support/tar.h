// -*- c++ -*-

#ifndef PEOS2_SUPPORT_TAR_H
#define PEOS2_SUPPORT_TAR_H

#include <stddef.h>
#include <stdint.h>

struct tar_entry {
  char name[100];
  char mode[8];
  char uid[8];
  char gid[8];
  char size[12];
  char mtime[12];
  char chksum[8];
  char type;
  char linkname[100];
  char magic[6];
  char tail[249];
};

uint32_t tar_parse_octal(const char *str, size_t length);

#endif // !PEOS2_SUPPORT_TAR_H

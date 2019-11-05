// -*- c++ -*-

#ifndef PEOS2_SUPPORT_TAR_H
#define PEOS2_SUPPORT_TAR_H

#include <stdint.h>
#include "support/string.h"
#include "support/pool.h"

struct tar_entry {
  p2::string<201> filename;
  uint16_t mode;
  uint16_t uid;
  uint16_t gid;
  uint32_t size;
  uint32_t mktime;
  uint16_t checksum;
  uint8_t type;
  const void *data;
};

typedef bool (*tar_parse_callback)(tar_entry &entry, void *opaque);

// TODO: clean up using lambdas
void tar_parse(const void *data, size_t size, tar_parse_callback callback_fun, void *opaque);

#endif // !PEOS2_SUPPORT_TAR_H

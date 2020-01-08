// -*- c++ -*-

#ifndef PEOS2_SUPPORT_FILESYSTEM_H
#define PEOS2_SUPPORT_FILESYSTEM_H

#include "support/string.h"

namespace p2 {

template<int N>
void dirname(const p2::string<N> &path, p2::string<N> *dir, p2::string<N> *base) {
  int last_slash = path.find_last_of('/');
  if (dir)
    *dir = p2::string<N>(path.c_str(), last_slash);

  if (base)
    *base = p2::string<N>(path.c_str() + last_slash + 1);
}
}

#endif // !PEOS2_SUPPORT_FILESYSTEM_H

// -*- c++ -*-

#ifndef PEOS2_SUPPORT_LIMITS_H
#define PEOS2_SUPPORT_LIIMTS_H

#include <stdint.h>

namespace p2 {
  template<typename T>
  struct numeric_limits {
    constexpr static T max();
  };

  template<> constexpr uint8_t  numeric_limits<uint8_t >::max() {return UINT8_MAX; }
  template<> constexpr uint16_t numeric_limits<uint16_t>::max() {return UINT16_MAX; }
  template<> constexpr uint32_t numeric_limits<uint32_t>::max() {return UINT32_MAX; }
  template<> constexpr uint64_t numeric_limits<uint64_t>::max() {return UINT64_MAX; }
}

#endif // !PEOS2_SUPPORT_LIMITS_H

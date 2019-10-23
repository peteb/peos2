// -*- c++ -*-

#ifndef PEOS2_SUPPORT_POOL_H
#define PEOS2_SUPPORT_POOL_H

#include <stdint.h>

namespace p2 {
  // Allocation time: O(1)
  // Deletion time: O(1)
  template<typename T, uint16_t _MaxLen>
  class pool {
    struct node {
      T value;
      uint16_t next_free;
    };

  public:
    uint16_t push_back(const T &value) {
      uint16_t idx;

      if (_freelist_head) {
        // We've got items on the freelist which we can use
        idx = _freelist_head;
        _freelist_head = _elements[idx].next_free;
        _elements[idx].next_free = 0;
      }
      else {
        idx = _watermark++;
      }

      assert(idx + 1 <= _MaxLen && "buffer overrun");
      _elements[idx] = {value, 0};
      ++_count;
      return idx;
    }

    void erase(uint16_t idx) {
      // If it's the final item we can just simplify things and
      // decrease the watermark
      assert(_watermark > 0);

      if (idx == _watermark - 1) {
        --_watermark;
        --_count;
        return;
      }

      // Add the item to the freelist
      _elements[idx].next_free = _freelist_head;
      _freelist_head = idx;
      --_count;
    }

    T &operator [](uint16_t idx) {
      assert(idx < _MaxLen && "buffer overrun");
      return _elements[idx].value;
    }

    const T &operator [](uint16_t idx) const {
      assert(idx < _MaxLen && "buffer overrun");
      return _elements[idx].value;
    }

    uint16_t size() const {
      return _count;
    }

  private:
    uint16_t _watermark = 0, _freelist_head = 0, _count = 0;
    node _elements[_MaxLen];
  };
}

#endif // !PEOS2_SUPPORT_POOL_H

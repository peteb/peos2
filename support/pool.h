// -*- c++ -*-

#ifndef PEOS2_SUPPORT_POOL_H
#define PEOS2_SUPPORT_POOL_H

#include <stdint.h>
#include <stddef.h>
#include "assert.h"
#include "support/limits.h"

namespace p2 {
  // Pool allocator with a linked list freelist. A benefit of the
  // linked list is that the next pointers are next to the element
  // data, leading to fewer cache misses. This solution been measured
  // to be faster than a stack based allocator but more tests need to
  // be done to conclusively say so.
  //
  // Time complexity: push_back:
  // O(1) erase: O(1)
  template<typename T, size_t _MaxLen, typename _IndexT = uint16_t>
  class pool {
    struct node {
      T value;
      _IndexT next_free;
    };

  public:
    pool() {
      // TODO: make these checks compile-time
      assert(_MaxLen <= p2::numeric_limits<_IndexT>::max() && "max value of _IndexT is reserved as a sentinel");
      assert(_MaxLen > 0);
    }

    _IndexT push_back(const T &value) {
      _IndexT idx;

      if (_freelist_head != END_SENTINEL) {
        // We've got items on the freelist which we can use
        idx = _freelist_head;
        _freelist_head = _elements[idx].next_free;
        _elements[idx].next_free = END_SENTINEL;
      }
      else {
        idx = _watermark++;
      }

      assert(idx < _MaxLen && "buffer overrun");
      _elements[idx] = {value, 0};
      ++_count;
      return idx;
    }

    void erase(_IndexT idx) {
      assert(_watermark > 0);
      assert(idx < _watermark);

      // If it's the final item we can just simplify things and
      // decrease the watermark
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

    T &operator [](_IndexT idx) {
      assert(idx < _MaxLen && "buffer overrun");
      return _elements[idx].value;
    }

    const T &operator [](_IndexT idx) const {
      assert(idx < _MaxLen && "buffer overrun");
      return _elements[idx].value;
    }

    size_t size() const {
      return _count;
    }

    _IndexT end() const {
      return END_SENTINEL;
    }

  private:
    _IndexT _watermark = 0, _freelist_head = END_SENTINEL, _count = 0;
    node _elements[_MaxLen];

    static const _IndexT END_SENTINEL = p2::numeric_limits<_IndexT>::max();
  };
}

#endif // !PEOS2_SUPPORT_POOL_H

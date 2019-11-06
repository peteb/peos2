// -*- c++ -*-

#ifndef PEOS2_SUPPORT_POOL_H
#define PEOS2_SUPPORT_POOL_H

#include <stdint.h>
#include <stddef.h>
#include "assert.h"
#include "support/limits.h"
#include "support/utils.h"

namespace p2 {
  //
  // Pool allocator with a linked list freelist. A benefit of the
  // linked list is that the next pointers are next to the element
  // data, leading to fewer cache misses. This solution has been
  // measured to be faster than a stack based allocator but more tests
  // need to be done to conclusively say so.
  //
  // Time complexity:
  // push_back: O(1)
  // erase:     O(1)
  //
  template<typename T, size_t _MaxLen, typename _IndexT = uint16_t>
  class pool {
    struct node {
      template<typename... _Args>
      node(_Args&&... args)
        : next_free(END_SENTINEL)
      {
        new (value()) T(forward<_Args>(args)...);
      }

      node(const T &o)
        : next_free(END_SENTINEL)
      {
        new (value()) T(o);
      }

      T *value()
      {
        return (T *)_value;
      }

      char _value[sizeof(T)] alignas(T);
      _IndexT next_free;
    };

  public:
    pool()
    {
      // TODO: make these checks compile-time
      assert(_MaxLen <= p2::numeric_limits<_IndexT>::max() && "max value of _IndexT is reserved as a sentinel");
      assert(_MaxLen > 0);
    }

    _IndexT push_back(const T &value)
    {
      return emplace_back(value);
    }

    template<typename... _Args>
    _IndexT emplace_back(_Args&&... args)
    {
      _IndexT idx;

      if (_freelist_head != END_SENTINEL) {
        // We've got items on the freelist which we can use
        idx = _freelist_head;
        _freelist_head = element(idx)->next_free;
        if (_freelist_head == END_SENTINEL)
          _freelist_tail = END_SENTINEL;

        element(idx)->next_free = END_SENTINEL;
      }
      else {
        idx = _watermark++;
      }

      assert(idx < _MaxLen && "buffer overrun");
      new (element(idx)) node(forward<_Args>(args)...);
      ++_count;
      return idx;
    }

    void erase(_IndexT idx)
    {
      assert(valid(idx));

      // If it's the final item we can just simplify things and
      // decrease the watermark
      if (idx == _watermark - 1) {
        --_watermark;
        --_count;
        return;
      }

      // Add the item to the freelist
      element(idx)->next_free = _freelist_head;
      _freelist_head = idx;

      if (_freelist_tail == END_SENTINEL)
        _freelist_tail = idx;

      --_count;
    }

    bool valid(_IndexT idx)
    {
      if (idx >= _watermark)
        return false;

      if (element(idx)->next_free != END_SENTINEL)
        return false;

      if (idx == _freelist_head)
        return false;

      if (idx == _freelist_tail)
        return false;

      return true;
    }

    T &operator [](_IndexT idx)
    {
      assert(idx < _MaxLen && "buffer overrun");
      return *element(idx)->value();
    }

    const T &operator [](_IndexT idx) const
    {
      assert(idx < _MaxLen && "buffer overrun");
      return *element(idx)->value();
    }

    size_t size() const
    {
      return _count;
    }

    _IndexT watermark()
    {
      return _watermark;
    }

    _IndexT end() const
    {
      return END_SENTINEL;
    }

  private:
    node *element(_IndexT idx)
    {
      return (node *)_element_data + idx;
    }

    _IndexT _watermark = 0,
      _freelist_head = END_SENTINEL,
      _freelist_tail = END_SENTINEL,
      _count = 0;

    char _element_data[_MaxLen * sizeof(node)] alignas(node);

    static const _IndexT END_SENTINEL = p2::numeric_limits<_IndexT>::max();
  };
}

#endif // !PEOS2_SUPPORT_POOL_H

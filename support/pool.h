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
  // Pool allocator with a linked list free list. A benefit of the
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
        : next_free(END_SENTINEL), prev_free(END_SENTINEL)
      {
        new (value()) T(forward<_Args>(args)...);
      }

      node(const T &o)
        : next_free(END_SENTINEL), prev_free(END_SENTINEL)
      {
        new (value()) T(o);
      }

      T *value()
      {
        return (T *)_value;
      }

      const T *value() const
      {
        return (T *)_value;
      }

      char _value[sizeof(T)] alignas(T);
      _IndexT next_free, prev_free;
      // prev_free is needed for `emplace`, which might un-free an
      // item in the middle of the free list. NB: _free_list_tail is
      // still needed for other things and isn't related to the
      // double-linked list.
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

    // TODO: rename this function: we're not necessarily pushing on
    // the "back".
    template<typename... _Args>
    _IndexT emplace_back(_Args&&... args)
    {
      _IndexT idx;

      if (_free_list_head != END_SENTINEL) {
        // We've got items on the free list which we can use
        idx = _free_list_head;
      }
      else {
        idx = _watermark;
      }

      emplace(idx, forward<_Args>(args)...);
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

      free_list_prepend(idx);
      --_count;
    }

    bool valid(_IndexT idx) const
    {
      if (idx >= _watermark)
        return false;

      if (element(idx)->next_free != END_SENTINEL)
        return false;

      if (idx == _free_list_head)
        return false;

      if (idx == _free_list_tail)
        return false;

      return true;
    }

    template<typename... _Args>
    void emplace(_IndexT idx, _Args&&... args)
    {
      resize(idx);

      if (valid(idx)) {
        // Something's in there already, just write over
        new (element(idx)) node(forward<_Args>(args)...);
        return;
      }

      // element[idx] is either on the free list or == watermark
      if (idx == _watermark) {
        ++_watermark;
      }
      else {
        // Remove element from the free list
        _IndexT next_free = element(idx)->next_free;
        _IndexT prev_free = element(idx)->prev_free;

        if (prev_free != END_SENTINEL)
          element(prev_free)->next_free = next_free;

        if (next_free != END_SENTINEL)
          element(next_free)->prev_free = prev_free;

        if (_free_list_head == idx)
          _free_list_head = next_free;

        if (_free_list_tail == idx)
          _free_list_tail = prev_free;

        element(idx)->prev_free = element(idx)->next_free = END_SENTINEL;
      }

      new (element(idx)) node(forward<_Args>(args)...);
      ++_count;
    }

    T &operator [](_IndexT idx)
    {
      assert(valid(idx));
      return *element(idx)->value();
    }

    const T &operator [](_IndexT idx) const
    {
      assert(valid(idx));
      return *element(idx)->value();
    }

    T *at(_IndexT idx)
    {
      if (!valid(idx))
        return nullptr;

      return element(idx)->value();
    }

    const T *at(_IndexT idx) const
    {
      if (!valid(idx))
        return nullptr;

      return element(idx)->value();
    }

    size_t size() const
    {
      return _count;
    }

    bool full() const
    {
      return _count >= _MaxLen - 1;
    }

    _IndexT watermark() const
    {
      return _watermark;
    }

    _IndexT end() const
    {
      return END_SENTINEL;
    }

  private:
    // Updates the watermark to `idx`, similar to calling `push_back`
    // and then immediately erasing the items
    void resize(_IndexT idx)
    {
      assert(idx < _MaxLen);

      while (_watermark < idx) {
        free_list_prepend(_watermark);
        ++_watermark;
      }
    }

    node *element(_IndexT idx)
    {
      return p2::launder(reinterpret_cast<node *>(_element_data)) + idx;
    }

    const node *element(_IndexT idx) const
    {
      return p2::launder(reinterpret_cast<const node *>(_element_data)) + idx;
    }

    void free_list_prepend(_IndexT idx)
    {
      if (_free_list_head != END_SENTINEL)
        element(_free_list_head)->prev_free = idx;

      element(idx)->next_free = _free_list_head;
      _free_list_head = idx;

      if (_free_list_tail == END_SENTINEL)
        _free_list_tail = idx;
    }

    _IndexT _watermark = 0,
      _free_list_head = END_SENTINEL,
      _free_list_tail = END_SENTINEL,
      _count = 0;

    char _element_data[_MaxLen * sizeof(node)] alignas(node);

    static const _IndexT END_SENTINEL = p2::numeric_limits<_IndexT>::max();
  };
}

#endif // !PEOS2_SUPPORT_POOL_H

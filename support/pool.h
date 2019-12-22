// -*- c++ -*-

#ifndef PEOS2_SUPPORT_POOL_H
#define PEOS2_SUPPORT_POOL_H

#include <stdint.h>
#include <stddef.h>

#if __STDC_HOSTED__ == 1
#include <iostream>
#endif // __STDC_HOSTED__ == 1

#include "support/assert.h"
#include "support/limits.h"
#include "support/utils.h"

namespace p2 {

// Pool allocator with a linked list free list. A benefit of the
// linked list is that the next pointers are next to the element
// data, leading to fewer cache misses. This solution has been
// measured to be faster than a stack based allocator but more tests
// need to be done to conclusively say so.
//
// NB: no destructors are called
//
// Time complexity:
// emplace:   O(1)
// erase:     O(1)
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
      return p2::launder((T *)_value);
    }

    const T *value() const
    {
      return p2::launder((T *)_value);
    }

    char _value[sizeof(T)] alignas(T);
    _IndexT next_free, prev_free;
    // prev_free is needed for `emplace`, which might un-free an
    // item in the middle of the free list. NB: _free_list_tail is
    // still needed for other things and isn't related to the
    // double-linked list.
  };

public:
  // A special iterator is needed due to the possibility of gap indexes -- positions
  // that don't have a value
  template<typename _ItemT, typename _IterT, typename _ContainerT>
  class iterator_base {
  public:
    _ItemT &operator *() const
    {
      return (*_container)[_idx];
    }

    _ItemT *operator ->() const
    {
      return &(*_container)[_idx];
    }

    _IterT operator ++()
    {
      _IterT copy(*static_cast<_IterT *>(this));
      ++_idx;
      jump_to_valid();
      return copy;
    }

    bool operator ==(const _IterT &other) const
    {
      return _container == other._container && _idx == other._idx;
    }

    bool operator !=(const _IterT &other) const
    {
      return !(*this == other);
    }

    _IterT &operator =(const _IterT &rhs)
    {
      _container = rhs._container;
      _idx = rhs._idx;
      return *this;
    }

    _IndexT index() const
    {
      return _idx;
    }

#if __STDC_HOSTED__ == 1
    // Lets hosted unit tests write out the expected and actual values
    friend std::ostream &operator <<(std::ostream &out, const _IterT &rhs)
    {
      out << rhs._idx;
      return out;
    }
#endif // __STDC_HOSTED__ == 1

  private:
    iterator_base(_ContainerT *container, _IndexT idx) : _container(container), _idx(idx) {jump_to_valid(); }
    iterator_base(const _IterT &other) : _container(other._container), _idx(other._idx) {}

    // Jumps over gaps in indexes and stops at the watermark. Cannot decrease idx
    void jump_to_valid()
    {
      while (_idx != _container->watermark() && !_container->valid(_idx))
        ++_idx;
    }

    _ContainerT *_container;
    _IndexT _idx;

    friend class pool;
  };

  class iterator : public iterator_base<T, iterator, pool> {
    using iterator_base<T, iterator, pool>::iterator_base;
  };

  class const_iterator : public iterator_base<const T, const_iterator, const pool> {
    using iterator_base<const T, const_iterator, const pool>::iterator_base;
  };

  pool()
  {
    // TODO: make these checks compile-time
    assert(_MaxLen <= p2::numeric_limits<_IndexT>::max() && "max value of _IndexT is reserved as a sentinel");
    assert(_MaxLen > 0);
    clear();
  }

  void clear()
  {
    _watermark = 0;
    _free_list_head = END_SENTINEL;
    _free_list_tail = END_SENTINEL;
    _count = 0;
  }

  // Picks a free position and assigns it. The free list
  // is checked first to avoid having to increase the watermark,
  // leading to more compact space usage. If no items have been erased,
  // allocated indexes will be continuously increasing by 1
  template<typename... _Args>
  _IndexT emplace_anywhere(_Args&&... args)
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

  // Returns the highest possible value, useful as a null index due to
  // the pool being 0-indexed
  _IndexT end_sentinel() const
  {
    return END_SENTINEL;
  }

  iterator begin()             {return iterator(this, 0); }
  iterator end()               {return iterator(this, _watermark); }
  const_iterator begin() const {return const_iterator(this, 0); }
  const_iterator end() const   {return const_iterator(this, _watermark); }

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

} // !p2

#endif // !PEOS2_SUPPORT_POOL_H

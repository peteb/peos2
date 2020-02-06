// -*- c++ -*-

#pragma once

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
      node(_Args&&... args) : _value(p2::forward<_Args>(args)...) {}
      node(const T &o) : _value(o) {}

      T *value() {return _value.get(); }

      const T *value() const {return _value.get(); }

      inplace_object<T> _value;

      // prev_free is needed for `emplace`, which might un-free an
      // item in the middle of the free list. NB: _free_list_tail is
      // still needed for other things and isn't related to the
      // double-linked list.
      _IndexT next_free = END_SENTINEL, prev_free = END_SENTINEL;
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

    static_assert(_MaxLen <= p2::numeric_limits<_IndexT>::max(), "max value of _IndexT is reserved as a sentinel");
    static_assert(_MaxLen > 0);

    pool() {clear(); }

    void clear();

    // Picks a free position and assigns it. The free list
    // is checked first to avoid having to increase the watermark,
    // leading to more compact space usage. If no items have been erased,
    // allocated indexes will be continuously increasing by 1
    template<typename... _Args> _IndexT emplace_anywhere(_Args&&... args);
    template<typename... _Args> void emplace(_IndexT idx, _Args&&... args);

    void erase(_IndexT idx);
    void erase(const iterator &it) {erase(it._idx); }

    bool valid(_IndexT idx) const;

    T &operator [](_IndexT idx);
    T *at(_IndexT idx);
    const T &operator [](_IndexT idx) const;
    const T *at(_IndexT idx) const;

    size_t size() const          {return _count; }
    bool full() const            {return _count >= _MaxLen - 1; }
    _IndexT watermark() const    {return _watermark; }
    _IndexT end_sentinel() const {return END_SENTINEL; }

    iterator begin()             {return iterator(this, 0); }
    iterator end()               {return iterator(this, _watermark); }
    const_iterator begin() const {return const_iterator(this, 0); }
    const_iterator end() const   {return const_iterator(this, _watermark); }

  private:
    // Updates the watermark to `idx`, similar to calling `emplace_anywhere`
    // and then immediately erasing the items
    void increase_size(_IndexT idx);
    void prepend_to_free_list(_IndexT idx);

    _IndexT _watermark = 0,
      _free_list_head = END_SENTINEL,
      _free_list_tail = END_SENTINEL,
      _count = 0;

    inplace_array<node, _MaxLen> _data;

    // The highest possible value, useful as a null index due to the
    // pool being 0-indexed
    static const _IndexT END_SENTINEL = p2::numeric_limits<_IndexT>::max();
  };


  template<typename T, size_t _MaxLen, typename _IndexT>
  void pool<T, _MaxLen, _IndexT>::clear()
  {
    _watermark = 0;
    _free_list_head = END_SENTINEL;
    _free_list_tail = END_SENTINEL;
    _count = 0;
  }

  template<typename T, size_t _MaxLen, typename _IndexT>
  template<typename... _Args>
  _IndexT pool<T, _MaxLen, _IndexT>::emplace_anywhere(_Args&&... args)
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

  template<typename T, size_t _MaxLen, typename _IndexT>
  void pool<T, _MaxLen, _IndexT>::erase(_IndexT idx)
  {
    assert(valid(idx));

    // If it's the final item we can just simplify things and
    // decrease the watermark
    if (idx == _watermark - 1) {
      --_watermark;
      --_count;
      return;
    }

    prepend_to_free_list(idx);
    --_count;
  }

  template<typename T, size_t _MaxLen, typename _IndexT>
  template<typename... _Args>
  void pool<T, _MaxLen, _IndexT>::emplace(_IndexT idx, _Args&&... args)
  {
    increase_size(idx);

    if (valid(idx)) {
      // Something's in there already, just write over
      _data.emplace(idx, forward<_Args>(args)...);
      return;
    }

    // element[idx] is either on the free list or == watermark
    if (idx == _watermark) {
      ++_watermark;
    }
    else {
      // Remove element from the free list
      _IndexT next_free = _data[idx].next_free;
      _IndexT prev_free = _data[idx].prev_free;

      if (prev_free != END_SENTINEL)
        _data[prev_free].next_free = next_free;

      if (next_free != END_SENTINEL)
        _data[next_free].prev_free = prev_free;

      if (_free_list_head == idx)
        _free_list_head = next_free;

      if (_free_list_tail == idx)
        _free_list_tail = prev_free;

      _data[idx].prev_free = _data[idx].next_free = END_SENTINEL;
    }

    _data.emplace(idx, forward<_Args>(args)...);
    ++_count;
  }

  template<typename T, size_t _MaxLen, typename _IndexT>
  bool pool<T, _MaxLen, _IndexT>::valid(_IndexT idx) const
  {
    if (idx >= _watermark)
      return false;

    if (_data[idx].next_free != END_SENTINEL)
      return false;

    if (idx == _free_list_head)
      return false;

    if (idx == _free_list_tail)
      return false;

    return true;
  }

  template<typename T, size_t _MaxLen, typename _IndexT>
  T &pool<T, _MaxLen, _IndexT>::operator [](_IndexT idx)
  {
    assert(valid(idx));
    return *_data[idx].value();
  }

  template<typename T, size_t _MaxLen, typename _IndexT>
  const T &pool<T, _MaxLen, _IndexT>::operator [](_IndexT idx) const
  {
    assert(valid(idx));
    return *_data[idx].value();
  }

  template<typename T, size_t _MaxLen, typename _IndexT>
  T *pool<T, _MaxLen, _IndexT>::at(_IndexT idx)
  {
    if (!valid(idx))
      return nullptr;

    return _data[idx].value();
  }

  template<typename T, size_t _MaxLen, typename _IndexT>
  const T *pool<T, _MaxLen, _IndexT>::at(_IndexT idx) const
  {
    if (!valid(idx))
      return nullptr;

    return _data[idx].value();
  }

  template<typename T, size_t _MaxLen, typename _IndexT>
  void pool<T, _MaxLen, _IndexT>::increase_size(_IndexT idx)
  {
    assert(idx < _MaxLen);

    while (_watermark < idx) {
      prepend_to_free_list(_watermark);
      ++_watermark;
    }
  }

  template<typename T, size_t _MaxLen, typename _IndexT>
  void pool<T, _MaxLen, _IndexT>::prepend_to_free_list(_IndexT idx)
  {
    if (_free_list_head != END_SENTINEL)
      _data[_free_list_head].prev_free = idx;

    _data[idx].next_free = _free_list_head;
    _free_list_head = idx;

    if (_free_list_tail == END_SENTINEL)
      _free_list_tail = idx;
  }

} // !p2


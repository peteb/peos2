// -*- c++ -*-

#ifndef PEOS2_SUPPORT_QUEUE_H
#define PEOS2_SUPPORT_QUEUE_H

namespace p2 {
  //
  // Basic FIFO
  //
  // _write_idx is always >= _read_idx, this makes it easier to check
  // size, look for overruns, etc, but we need to normalize the
  // indexes lest we'll get integer overflows.
  //
  // This queue isn't meant to be used with very large _MaxLen.
  //
  template<typename T, int _MaxLen>
  class queue {
  public:
    // TODO: use placement new, right now we're doing a lot of initialization

    // Returns false if the queue is full
    bool push_back(const T &val)
    {
      if (_write_idx - _read_idx >= _MaxLen) {
        return false;
      }

      _items[_write_idx++ % _MaxLen] = val;
      normalize();
      return true;
    }

    bool replace(int offset, const T &val)
    {
      while (_read_idx + offset >= _write_idx && push_back(T()));
      if (_read_idx + offset > _write_idx)
        return false;

      _items[(_read_idx + offset) % _MaxLen] = val;
      return true;
    }

    // Don't call this if size() == 0
    T pop_front()
    {
      return _items[_read_idx++ % _MaxLen];
    }

    const T &front() const
    {
      return _items[_read_idx % _MaxLen];
    }

    int size() const
    {
      return _write_idx - _read_idx;
    }

    bool full() const
    {
      return size() == _MaxLen;
    }

    int begin() const
    {
      return _read_idx;
    }

    int end() const
    {
      return _write_idx;
    }

    const T &operator [](int idx) const
    {
      return _items[idx % _MaxLen];
    }

    void clear()
    {
      _read_idx = _write_idx = 0;
    }

  private:
    void normalize()
    {
      int smallest = _read_idx / _MaxLen;
      _read_idx -= smallest * _MaxLen;
      _write_idx -= smallest * _MaxLen;
    }

    T _items[_MaxLen];
    int _read_idx = 0, _write_idx = 0;
  };
}

#endif // !PEOS2_SUPPORT_QUEUE_H

// -*- c++ -*-

#ifndef PEOS2_SUPPORT_OPTIONAL_H
#define PEOS2_SUPPORT_OPTIONAL_H

#include "support/utils.h"
#include "assert.h"

namespace p2 {
  template<typename T>
  class optional {
  public:
    optional() {}

    template<typename... _Args>
    optional(_Args&&... args)
    {
      new (_storage) T(forward<_Args>(args)...);
      _is_assigned = true;
    }

    T &operator *()
    {
      assert(_is_assigned);
      return *reinterpret_cast<T *>(_storage);
    }

    const T &operator *() const
    {
      assert(_is_assigned);
      return *reinterpret_cast<const T *>(_storage);
    }

    T *operator ->()
    {
      assert(_is_assigned);
      return reinterpret_cast<T *>(_storage);
    }

    const T *operator ->() const
    {
      assert(_is_assigned);
      return reinterpret_cast<const T *>(_storage);
    }

    explicit operator bool() const
    {
      return _is_assigned;
    }

    template<typename... _Args>
    T value_or(_Args&&... args)
    {
      if (*this)
        return this->operator *();
      else
        return T(forward<_Args>(args)...);
    }

  private:
    char _storage[sizeof(T)] alignas(T);
    bool _is_assigned = false;
  };

  template<typename T>
  using opt = optional<T>;
}

#endif // !PEOS2_SUPPORT_OPTIONAL_H

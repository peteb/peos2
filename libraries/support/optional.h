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
    : _is_assigned(true)
  {
    _value.construct(p2::forward<_Args>(args)...);
  }

  T &operator *()
  {
    assert(_is_assigned);
    return *_value;
  }

  const T &operator *() const
  {
    assert(_is_assigned);
    return *_value;
  }

  T *operator ->()
  {
    assert(_is_assigned);
    return _value.get();
  }

  const T *operator ->() const
  {
    assert(_is_assigned);
    return _value.get();
  }

  explicit operator bool() const
  {
    return _is_assigned;
  }

  template<typename... _Args>
  T value_or(_Args&&... args)
  {
    if (*this)
      return **this;
    else
      return T(forward<_Args>(args)...);
  }

private:
  inplace_object<T> _value;
  bool _is_assigned = false;
};

template<typename T>
using opt = optional<T>;
}

#endif // !PEOS2_SUPPORT_OPTIONAL_H

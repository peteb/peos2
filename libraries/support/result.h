// -*- c++ -*-
//
// result.h - a similar thing to optional, but with an error code
//

#ifndef PEOS2_SUPPORT_RESULT_H
#define PEOS2_SUPPORT_RESULT_H

#include "support/utils.h"
#include "support/assert.h"

#define bubble(result) if (!(result)) {return p2::failure((result).error());}

namespace p2 {
  class failure {
  public:
    failure(int code) : _code(code) {}

    // For casting to int in functions that only return an error code
    operator int() const {return _code; }

    int _code;
  };

  template<typename T>
  class success {
  public:
    success(T value) : value(value) {}
    T value;
  };

  template<typename T>
  class result {
  public:
    result() = delete;

    result(const failure &f)
      : _error(f._code)
    {}

    result(const success<T> &&val)
    {
      new (_storage) T(val.value);
      _successful = true;
    }

    T &operator *()
    {
      assert(_successful);
      return *p2::launder(reinterpret_cast<T *>(_storage));
    }

    const T &operator *() const
    {
      assert(_successful);
      return *p2::launder(reinterpret_cast<const T *>(_storage));
    }

    explicit operator bool() const
    {
      return _successful;
    }

    int error() const
    {
      return _error;
    }

  private:
    char _storage[sizeof(T)] alignas(T);
    bool _successful = false;
    int _error = 0;
  };

  template<typename T>
  using res = result<T>;
}

#endif // !PEOS2_SUPPORT_RESULT_H

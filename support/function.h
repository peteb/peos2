// -*- c++ -*-

#ifndef PEOS2_SUPPORT_FUNCTION_H
#define PEOS2_SUPPORT_FUNCTION_H

#include "utils.h"
#include "assert.h"

namespace p2 {
  template<size_t, typename>
  class inline_fun;

  // Stores the lambda inline, no heap allocations
  template<size_t _MaxSize, typename _RetVal, typename... _Args>
  class inline_fun<_MaxSize, _RetVal(_Args...)> {
  public:
    template<typename T>
    inline_fun(T t) {
      *this = t;
    }

    template<typename T>
    inline_fun &operator =(T t) {
      // TODO: assert that there's space for the needed alignment as well
      assert(sizeof(callable_impl<T>) <= sizeof(_storage));
      new (_storage) callable_impl<T>(t);
      return *this;
    }

    _RetVal operator()(_Args&&... args) {
      callable *target = p2::launder(reinterpret_cast<callable *>(_storage));
      return target->invoke(p2::forward<_Args>(args)...);
    }

  private:
    class callable {
    public:
      virtual ~callable() = default;
      virtual _RetVal invoke(_Args&&...) = 0;
    };

    template<typename T>
    class callable_impl : public callable {
    public:
      callable_impl(const T &lambda) : _lambda(lambda) {}
      ~callable_impl() override = default;

      _RetVal invoke(_Args&&... args) override {
        return _lambda(p2::forward<_Args>(args)...);
      }

    private:
      T _lambda;
    };

    char _storage[_MaxSize];
  };

}

#endif // !PEOS2_SUPPORT_FUNCTION_H

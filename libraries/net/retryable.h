// -*- c++ -*-

#ifndef NET_RETRYABLE_H
#define NET_RETRYABLE_H

#include <support/function.h>
#include <support/pool.h>

// Wraps a lambda in retry logic with support for
// registering listeners that will be called when a result exists
template<typename _Result>
class retryable {
public:
  using await_fun = p2::inplace_fun<64, void(_Result)>;
  using op_fun = p2::inplace_fun<32, void()>;

  retryable(op_fun fun, int retry_ms = 200)
    : _fun(fun)
  {
    _last_delta = retry_ms;
  }

  bool tick(int dt)
  {
    _retry_timer -= dt;

    if (_retry_timer <= 0) {
      if (_retries++ > 4)
        return false;

      _last_delta *= 2;
      _retry_timer = _last_delta;
      _fun();
    }

    return true;
  }

  void reset()
  {
    _retry_timer /= 2;
    _last_delta /= 2;
    _retries = 0;
  }

  void notify_all(const _Result &result)
  {
    for (auto &waiter : _waiters) {
      waiter(result);
    }
  }

  void await(const await_fun &fun)
  {
    _waiters.emplace_anywhere(fun);
  }

private:
  op_fun _fun;
  p2::fixed_pool<await_fun, 32> _waiters;

  int _last_delta = 0;
  int _retry_timer = 0;
  int _retries = 0;
};

#endif // !NET_RETRYABLE_H

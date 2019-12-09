// -*- c++ -*-

#ifndef NET_RETRYABLE_H
#define NET_RETRYABLE_H

#include <support/function.h>
#include <support/pool.h>

// retryable - wraps a lambda in retry logic with support for
// registering listeners that will be called when a result exists
template<typename _Result>
class retryable {
public:
  typedef p2::inline_fun<64, void(_Result)> await_fun;
  typedef p2::inline_fun<32, void()> op_fun;

  retryable(op_fun fun)
    : _fun(fun)
  {
    _last_delta = 10;
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
    for (size_t i = 0; i < _waiters.watermark(); ++i) {
      if (!_waiters.valid(i))
        continue;

      _waiters[i](result);
    }
  }

  void await(const await_fun &fun)
  {
    _waiters.emplace_anywhere(fun);
  }

private:
  op_fun _fun;
  p2::pool<await_fun, 32> _waiters;

  int _last_delta = 0;
  int _retry_timer = 0;
  int _retries = 0;
};

#endif // !NET_RETRYABLE_H

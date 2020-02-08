// -*- c++ -*-

#ifndef PEOS2_LOCKS_H
#define PEOS2_LOCKS_H

#include <stddef.h>

#include "support/pool.h"
#include "process.h"

template<size_t _MaxWaiters>
class condition_variable {
public:
  int wait()
  {
    // TODO: verify that we're not already on the list
    int idx = _waiting_procs.emplace_anywhere(*proc_current_pid());
    int ret = proc_block(*proc_current_pid());

    if (ret < 0 && _waiting_procs.valid(idx))
      _waiting_procs.erase(idx);

    return ret;
  }

  void notify_one()
  {
    // TODO: make this more fair?
    for (size_t i = 0; i < _waiting_procs.watermark(); ++i) {
      if (!_waiting_procs.valid(i))
        continue;

      notify(i);
      break;
    }
  }

  void notify_all()
  {
    // TODO: make this more fair?
    for (size_t i = 0; i < _waiting_procs.watermark(); ++i) {
      if (!_waiting_procs.valid(i))
        continue;

      notify(i);
    }
  }

private:
  void notify(size_t idx)
  {
    // TODO: what if the process has exited?
    proc_unblock_and_switch(_waiting_procs[idx], 1);
    _waiting_procs.erase(idx);
  }

  p2::fixed_pool<proc_handle, _MaxWaiters> _waiting_procs;
};

#endif // !PEOS2_LOCKS_H

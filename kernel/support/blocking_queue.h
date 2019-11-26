// -*- c++ -*-
//
// I'm not sure whether this file should actually be in "support/" due
// to the dependencies on non-support files.
//

#ifndef PEOS2_SUPPORT_BLOCKING_QUEUE_H
#define PEOS2_SUPPORT_BLOCKING_QUEUE_H

#include <stddef.h>

#include "process.h"
#include "locks.h"
#include "support/queue.h"
#include "debug.h"

namespace p2 {
  //
  // Sngle-writer, single-reader blocking queue. Won't block the
  // writer if the queue is full. Typically, writers are IRQ
  // interrupts, so we can't just block them.
  //
  template<size_t _MaxLen>
  class blocking_data_queue {
  public:
    //
    // Returns how many bytes were pushed
    //
    size_t push_back(const char *data, size_t length)
    {
      size_t bytes_written = 0;

      for (; bytes_written < length; ++bytes_written) {
        // TODO: more efficient block pushing
        if (!_queue.push_back(data[bytes_written])) {
          break;
        }
      }

      if (bytes_written == 0) {
        return 0;
      }

      // As a single consumer might not read everything we've written,
      // we're notifying all of them
      _pushed_data_signal.notify_all();
      return bytes_written;
    }

    int pop_front(char *destination, int max_size)
    {
      if (max_size == 0)
        return 0;

      int bytes_read = 0;

      // We don't need a mutex around the state because mutual
      // exclusion is guaranteed by the IF bit in EFLAGS: no
      // concurrent interrupts will be invoked and there are no other
      // CPUs. For now.

      while (true) {
        if (_queue.size() == 0) {
          dbg_puts(blocking_queue, "waiting for cond");
          if (int ret = _pushed_data_signal.wait(); ret < 0) {
            dbg_puts(blocking_queue, "returning %d", ret);
            return ret;
          }
        }

        // Even though we've been woken up, the input queue might've
        // been emptied since then. That's what we've got the outer
        // loop for.
        while (_queue.size() > 0 && max_size > 0) {
          // TODO: block copy
          destination[bytes_read++] = _queue.pop_front();
          --max_size;
        }

        if (bytes_read > 0) {
          break;
        }
      }

      return bytes_read;
    }

    bool full() const
    {
      return _queue.full();
    }

  private:
    p2::queue<char, _MaxLen> _queue;
    condition_variable<16> _pushed_data_signal;
  };

}

#endif // !PEOS2_SUPPORT_BLOCKING_QUEUE_H

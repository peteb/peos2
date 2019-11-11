// -*- c++ -*-
//
// I'm not sure whether this file should actually be in "support/" due
// to the dependencies on non-support files.
//

#ifndef PEOS2_SUPPORT_BLOCKING_QUEUE_H
#define PEOS2_SUPPORT_BLOCKING_QUEUE_H

#include <stddef.h>

#include "process.h"

#include "support/queue.h"

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
    template<typename _CleanupFunT>
    size_t push_back(const char *data, size_t length, _CleanupFunT cleanup_fun) {
      asm volatile("cli");
      size_t bytes_written = 0;

      for (; bytes_written < length; ++bytes_written) {
        if (!_queue.push_back(data[bytes_written])) {
          break;
        }
      }

      if (bytes_written == 0) {
        // TODO: restore interrupt bit?
        return 0;
      }

      if (_process_waiting) {
        proc_resume(_waiting_process);
        _process_waiting = false;
        cleanup_fun();

        // TODO: can we actually do the process switch here? Wouldn't
        // it be better to let the interrupt handler continue its work
        // immediately and then switch task before the ISR executes iret?
        proc_switch(_waiting_process);
      }

      return bytes_written;
    };

    size_t pop_front(char *destination, size_t max_size) {
      int bytes_read = 0;

      while (true) {
        asm volatile("cli");

        if (_queue.size() == 0) {
          // We need to block!
          // TODO: check that there's none already waiting
          proc_suspend(*proc_current_pid());
          _waiting_process = *proc_current_pid();
          _process_waiting = true;

          proc_yield();
          // When the process is unsuspended, execution will continue here
        }

        asm volatile("cli");

        // Even though we've been woken up, the input_queue might've been
        // emptied since then. That's what we've got the outer loop for.
        while (_queue.size() > 0 && max_size > 0) {
          destination[bytes_read++] = _queue.pop_front();
          --max_size;
        }

        if (bytes_read > 0) {
          break;
        }
      }

      return bytes_read;
    }

  private:
    p2::queue<char, _MaxLen> _queue;
    proc_handle _waiting_process;
    bool _process_waiting = false;
  };

}

#endif // !PEOS2_SUPPORT_BLOCKING_QUEUE_H

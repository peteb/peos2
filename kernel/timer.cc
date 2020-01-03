#include <support/pool.h>

#include "timer.h"
#include "x86.h"
#include "protected_mode.h"
#include "debug.h"
#include "syscalls.h"
#include "syscall_utils.h"

// Externals
extern "C" void isr_timer(isr_registers *);

// Statics
static int syscall_currenttime(uint64_t *time_out);

// Global state
static p2::pool<timer_callback, 16> tick_callbacks;
static int milliseconds_per_tick = 0;
static uint64_t milliseconds_since_start = 0;

void timer_init()
{
  // For some reason, frequency = 10 doesn't work good at all, but 100
  // is quite accurate
  int frequency = 100;
  pit_set_phase(frequency);
  milliseconds_per_tick = 1000 / frequency;

  irq_enable(IRQ_SYSTEM_TIMER);
  int_register(IRQ_BASE_INTERRUPT + IRQ_SYSTEM_TIMER, isr_timer, KERNEL_CODE_SEL, IDT_TYPE_INTERRUPT|IDT_TYPE_D|IDT_TYPE_P);

  syscall_register(SYSCALL_NUM_CURRENTTIME, (syscall_fun)syscall_currenttime);
}

extern "C" void int_timer(isr_registers *)
{
  irq_eoi(IRQ_SYSTEM_TIMER);
  milliseconds_since_start += milliseconds_per_tick;

  for (auto &callback : tick_callbacks) {
    callback(milliseconds_per_tick);
  }
}

void timer_register_tick_callback(timer_callback callback)
{
  tick_callbacks.emplace_anywhere(callback);
}

static int syscall_currenttime(uint64_t *time_out)
{
  verify_ptr(timer, time_out);
  *time_out = milliseconds_since_start;
  return 0;
}

// -*- c++ -*-
#ifndef PEOS2_TIMER_H
#define PEOS2_TIMER_H

typedef void (*timer_callback)(int milliseconds);

void timer_init();
void timer_register_tick_callback(timer_callback callback);

#endif // !PEOS2_TIMER_H

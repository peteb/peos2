// -*- c++ -*-
//
// Implements context switch and everything related to the management
// of processes.
//

#ifndef PEOS2_PROCESS_H
#define PEOS2_PROCESS_H

#include <stdint.h>

typedef uint16_t proc_handle;

void        proc_init();
proc_handle proc_create(void *eip);
void        proc_switch(proc_handle pid);
void        proc_suspend(proc_handle pid);
void        proc_resume(proc_handle pid);
void        proc_yield();
proc_handle proc_current_pid();
void        proc_kill(proc_handle pid, uint32_t exit_status);

#endif // !PEOS2_PROCESS_H

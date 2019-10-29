// -*- c++ -*-
//
// Implements context switch and everything related to the management
// of processes. No scheduling logic.
//

#ifndef PEOS2_PROCESS_H
#define PEOS2_PROCESS_H

#include <stdint.h>

typedef uint16_t proc_handle;

void proc_init();
proc_handle proc_create(void *eip);
void proc_switch(proc_handle pid);

#endif // !PEOS2_PROCESS_H

// -*- c++ -*-
//
// Implements context switch and everything related to the management
// of processes.
//

#ifndef PEOS2_PROCESS_H
#define PEOS2_PROCESS_H

#include <stdint.h>
#include "memory.h"

#define PROC_USER_SPACE          0x01
#define PROC_KERNEL_ACCESSIBLE   0x02

typedef uint16_t proc_handle;

struct proc_fd {
  void *opaque;
  int value;
};

void        proc_init();
proc_handle proc_create(void *eip, uint32_t flags, const char *argument);
void        proc_switch(proc_handle pid);
void        proc_suspend(proc_handle pid);
void        proc_resume(proc_handle pid);
void        proc_yield();
proc_handle proc_current_pid();
void        proc_kill(proc_handle pid, uint32_t exit_status);

int         proc_create_fd(proc_handle pid, proc_fd fd);
proc_fd    *proc_get_fd(proc_handle pid, int fd);

mem_adrspc  proc_get_address_space(proc_handle pid);

#endif // !PEOS2_PROCESS_H

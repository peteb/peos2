// -*- c++ -*-
//
// Implements context switch and everything related to the management
// of processes.
//

#ifndef PEOS2_PROCESS_H
#define PEOS2_PROCESS_H

#include <stdint.h>
#include "memory.h"
#include "filesystem.h"
#include "support/optional.h"

#define PROC_USER_SPACE          0x01
#define PROC_KERNEL_ACCESSIBLE   0x02

typedef uint16_t proc_handle;

void                 proc_init();
proc_handle          proc_create(uint32_t flags, const char *argument);
void                 proc_enqueue(proc_handle pid);
void                 proc_switch(proc_handle pid);
void                 proc_suspend(proc_handle pid);
void                 proc_resume(proc_handle pid);
void                 proc_yield();
p2::opt<proc_handle> proc_current_pid();

void                 proc_kill(proc_handle pid, uint32_t exit_status);
mem_space            proc_get_space(proc_handle pid);
vfs_context          proc_get_file_context(proc_handle pid);
void                 proc_set_syscall_ret(proc_handle pid, uintptr_t ip);
void                 proc_assign_user_stack(proc_handle pid, const char *argv[], int argc);

#endif // !PEOS2_PROCESS_H

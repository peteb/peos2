// -*- c++ -*-

#ifndef PEOS2_PANIC_H
#define PEOS2_PANIC_H

// Terminates the currently executing context, for example
// a process executing a syscall, a hosted unittest, or
// the kernel itself
[[noreturn]] void panic(const char *explanation);

#endif // !PEOS2_PANIC_H

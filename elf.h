// -*- c++ -*-
//
// elf.h - parses and loads elf binaries into processes.
//
#ifndef PEOS2_ELF_H
#define PEOS2_ELF_H

#include "process.h"

int elf_map_process(const char *filename, proc_handle pid);

#endif // !PEOS2_ELF_H

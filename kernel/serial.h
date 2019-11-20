// -*- c++ -*-

#ifndef PEOS2_SERIAL_H
#define PEOS2_SERIAL_H

#include <stdint.h>
#include <stddef.h>

void com_init();
void com_send(const char *data, int count);
void com_setup_rx();

#endif // !PEOS2_SERIAL_H

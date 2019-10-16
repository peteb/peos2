#ifndef PEOS2_ASSERT_H
#define PEOS2_ASSERT_H

#include "panic.h"

#ifdef NDEBUG
#define ASSERT(exp) {if (!(exp)) {panic("Assertion failed: " #exp); } }
#else
#define ASSERT(exp)
#endif

#endif // !PEOS2_ASSERT_H

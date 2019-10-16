#ifndef PEOS2_ASSERT_H
#define PEOS2_ASSERT_H

#include "panic.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifdef NDEBUG
#define assert(exp) {if (!(exp)) {panic("ASSERT " __FILE__ ":" TOSTRING(__LINE__) ": " #exp); } }
#else
#define assert(exp)
#endif

#endif // !PEOS2_ASSERT_H

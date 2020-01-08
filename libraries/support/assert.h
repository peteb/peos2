#ifndef PEOS2_ASSERT_H
#define PEOS2_ASSERT_H

#include "panic.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#ifdef NDEBUG
#define assert(exp)                                                     \
  do {                                                                  \
    if (!(exp)) {                                                       \
      panic("ASSERT " __FILE__ ":" TOSTRING(__LINE__) ": " #exp);       \
    }                                                                   \
  } while(0)

#else
#define assert(exp)
#endif

#endif // !PEOS2_ASSERT_H

#include "panic.h"
#include "screen.h"

void panic(const char *explanation) {
  puts("PANIC! ");
  puts(explanation);
}

#include "panic.h"
#include "screen.h"

void panic(const char *explanation) {
  print("PANIC! ");
  print(explanation);
}

#include "string.h"
#include "assert.h"

char p2::digit_as_char(int digit, int radix) {
  if (radix == 10) {
    return '0' + digit;
  }
  else if (radix == 16) {
    if (digit < 10) {
      return '0' + digit;
    }
    else {
      return 'A' + digit - 10;
    }
  }

  return '?';
}

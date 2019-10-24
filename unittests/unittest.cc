#include "unittest.h"

#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define COLOR_RESET "\033[0m"

#if __STDC_HOSTED__ == 1
#include <iostream>
#include <iomanip>
#include <unistd.h>

static bool expect_panic = false;
static bool got_panic = false;

// These below are populated by ld for us, matches the start and end
// of the "TESTCASES" section
extern testcase_info __start_TESTCASES;
extern testcase_info __stop_TESTCASES;

int main() {
  const testcase_info *tc = &__start_TESTCASES;

  while (tc != &__stop_TESTCASES) {
    tc->fun();
    ++tc;
  }
}

void panic(const char *explanation) {
  if (expect_panic) {
    got_panic = true;
    expect_panic = false;
    return;
  }

  case_report(false, explanation);
}

void start_assert_panic() {
  expect_panic = true;
  got_panic = false;
}

void stop_assert_panic() {
  expect_panic = false;
  got_panic = false;
}

bool received_panic() {
  return got_panic;
}
#endif // __STDC_HOSTED__ == 1

static const char *current_case;

static std::ostream &green_header(const char *header_text) {
  std::cout << GREEN << header_text << " " << COLOR_RESET << " ";
  return std::cout;
}

static std::ostream &red_header(const char *header_text) {
  std::cout << RED << header_text << " " << COLOR_RESET << " ";
  return std::cout;
}

void suite_begin(const char *name) {
  green_header("[------------]") << name << std::endl;;
  current_case = nullptr;
}

void suite_end() {
  if (current_case) {
    case_report(true, "");
  }

  green_header("[------------]") << "all tests passed" << std::endl << std::endl;
}

void testcase(const char *name) {
  if (current_case) {
    case_report(true, "");
  }

  green_header("[ RUN        ]") << "  " << name << "\r" << std::flush;
  current_case = name;
}

void case_report(bool succeeded, const char *exp) {
  if (succeeded) {
    green_header("[         OK ]") << "  " << current_case << std::endl;
    current_case = nullptr;
  }
  else {
    red_header("[       FAIL ]") << "  " << current_case << ": " << exp << std::endl;
    asm("int 3");
    exit(1);
  }
}

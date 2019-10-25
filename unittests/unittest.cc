#include "unittest.h"

#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define COLOR_RESET "\033[0m"

#if __STDC_HOSTED__ == 1
#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <chrono>

static jmp_buf *panic_jmp_env = nullptr;
static std::chrono::high_resolution_clock::time_point testcase_start;

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
  if (panic_jmp_env) {
    longjmp(*panic_jmp_env, 1);
    return;
  }

  case_report(false, explanation);
}

void set_panic_jmp(jmp_buf *env) {
  panic_jmp_env = env;
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
  testcase_start = std::chrono::high_resolution_clock::now();
}

void case_report(bool succeeded, const char *exp) {
  std::chrono::duration<double> elapsed_time = std::chrono::high_resolution_clock::now() - testcase_start;

  if (succeeded) {
    if (elapsed_time.count() > 0.01) {
      green_header("[         OK ]") << "  " << current_case << " " << std::fixed << std::setprecision(0) << " (" << elapsed_time.count() * 1000.0 << " ms)" << std::endl;
    }
    else {
      green_header("[         OK ]") << "  " << current_case << std::endl;
    }

    current_case = nullptr;
  }
  else {
    red_header("[       FAIL ]") << "  " << current_case << ": " << exp << std::endl;
    asm("int 3");
    exit(1);
  }
}

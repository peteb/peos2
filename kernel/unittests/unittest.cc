#include "unittest.h"

#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <chrono>

#define RED "\033[1;31m"
#define GREEN "\033[1;32m"
#define COLOR_RESET "\033[0m"

static jmp_buf *panic_jmp_env;
static std::chrono::high_resolution_clock::time_point testcase_start;
static const char *current_case;

static void suite_begin(const char *name);
static void suite_end();

// These below are populated by ld for us, matches the start and end
// of the "TESTSUITES" section
extern testsuite_info __start_TESTSUITES;
extern testsuite_info __stop_TESTSUITES;

// Entry point when running in the host environment
int main() {
  const testsuite_info *ts = &__start_TESTSUITES;

  while (ts != &__stop_TESTSUITES) {
    suite_begin(ts->name);
    ts->fun();
    suite_end();
    ++ts;
  }
}

// Implements the `panic` function declared in the kernel so we can
// catch it in our tests. We need to use a longjmp here because the
// rest of the code expects `panic` to not return.
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
    abort();
  }
}

// -*- c++ -*-

#ifndef PEOS2_UNITTEST_H
#define PEOS2_UNITTEST_H

#include <sstream>

#define ASSERT_EQ(value, expected) assert_eq((value), (expected), __FILE__, __LINE__)
#define BEGIN_ASSERT_PANIC start_assert_panic()
#define ASSERT_PANIC() ASSERT_EQ(received_panic(), true)

void suite_begin(const char *name);
void suite_end();
void testcase(const char *name);
void case_report(bool succeeded, const char *exp);
void start_assert_panic();
void stop_assert_panic();
bool received_panic();

#ifdef __STDC_HOSTED__
void panic(const char *explanation);
#endif // !__STDC_HOSTED

template<typename A, typename B>
void assert_eq(const A &value, const B &expected, const char *file, int line) {
  if (value != expected) {
    std::stringstream ss;
    ss << "[" << file << ":" << line << "] expected " << value << " to equal " << expected;
    case_report(false, ss.str().c_str());
  }
}


#endif // !PEOS2_UNITTEST_H

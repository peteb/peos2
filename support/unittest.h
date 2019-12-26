// -*- c++ -*-

#ifndef PEOS2_UNITTEST_H
#define PEOS2_UNITTEST_H

#include <sstream>
#include <csetjmp>

#define BARRIER asm("":::"memory")

#define ASSERT_EQ(value, expected) assert_eq((value), (expected), __FILE__, __LINE__)
#define ASSERT_NEQ(value, expected) assert_neq((value), (expected), __FILE__, __LINE__)
#define ASSERT_TRUE(value) ASSERT_EQ(value, true)
#define ASSERT_FALSE(value) ASSERT_EQ(value, false)

#define ASSERT_PANIC(code) {                                            \
  jmp_buf env;                                                          \
  int paniced = setjmp(env);                                            \
  set_panic_jmp(&env);                                                  \
  if (paniced == 0) {                                                   \
    (code);                                                             \
    case_report(false, "no panic received");                            \
  }                                                                     \
  set_panic_jmp(nullptr);                                               \
}                                                                       \

#define TESTSUITE(name) static void suite();                            \
  __attribute__((section ("TESTSUITES"), used)) static testsuite_info ts_info = {suite, #name}; \
  static void suite()

#define TESTCASE(name) testcase(name); BARRIER;

typedef void (*testsuite_fun)() ;

struct testsuite_info {
  testsuite_fun fun;
  const char *name;
};

void testcase(const char *name);
void case_report(bool succeeded, const char *exp);
void set_panic_jmp(jmp_buf *env);

template<typename A, typename B>
void assert_eq(const A &value, const B &expected, const char *file, int line)
{
  if (!(value == expected)) {
    std::stringstream ss;
    ss << file << ":" << line << ": expected " << value << " to equal " << expected;
    case_report(false, ss.str().c_str());
  }
}

template<typename A, typename B>
void assert_neq(const A &value, const B &expected, const char *file, int line)
{
  if (!(value != expected)) {
    std::stringstream ss;
    ss << "[" << file << ":" << line << "] expected " << value << " to not equal " << expected;
    case_report(false, ss.str().c_str());
  }
}

#endif // !PEOS2_UNITTEST_H

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

#define BEGIN_SUITE(name) static void suite() { \
  suite_begin(#name);
#define END_SUITE suite_end(); }                                      \
  __attribute__((section ("TESTCASES"), used)) static testcase_info tc_info = {suite};
#define TESTCASE(name) testcase(name); BARRIER;

typedef void (*testcase_fun)() ;

struct testcase_info {
  testcase_fun fun;
};

void suite_begin(const char *name);
void suite_end();
void testcase(const char *name);
void case_report(bool succeeded, const char *exp);
void set_panic_jmp(jmp_buf *env);

template<typename A, typename B>
void assert_eq(const A &value, const B &expected, const char *file, int line) {
  if (!(value == expected)) {
    std::stringstream ss;
    ss << "[" << file << ":" << line << "] expected " << value << " to equal " << expected;
    case_report(false, ss.str().c_str());
  }
}

template<typename A, typename B>
void assert_neq(const A &value, const B &expected, const char *file, int line) {
  if (!(value != expected)) {
    std::stringstream ss;
    ss << "[" << file << ":" << line << "] expected " << value << " to not equal " << expected;
    case_report(false, ss.str().c_str());
  }
}

#endif // !PEOS2_UNITTEST_H

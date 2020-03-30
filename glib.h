// This is a stub used to run SaneC tests without GLib.

#include <stdio.h>
#include <assert.h>

#ifdef NDEBUG
#error This test cannot run with NDEBUG
#endif

#define g_assert_true \
  assert

#define g_test_fail() \
  assert(0)

#define g_assert_cmpstr(s1, op, s2) \
  assert(!strcmp(#op, "==")); \
  assert(!strcmp(s1, s2))

#define g_test_init(a, b, c)

#define g_test_add_func(path, func) \
  func()

#define g_test_run() \
  puts("all ok"), 0

/* saneex.c - A try..catch Implementation In Plain C (C99)
   by Proger_XP | https://github.com/ProgerXP/SaneC | public domain (CC0) */

/*
  These tests are using GLib test functions:
  https://developer.gnome.org/glib/unstable/glib-Testing.html

  If you have GLib then this will work:
  gcc -Wall -Wextra saneex-test.c saneex.c `pkg-config --cflags --libs glib-2.0`

  Else you can use the included glib.h stub:
  gcc -Wall -Wextra saneex-test.c saneex.c -I.
*/

#include <glib.h>
#include "saneex.h"

#define START   \
  char trace[11] = "\0\0\0\0\0" "\0\0\0\0\0" "\1"

#define PASS(c) \
  addPass(trace, #c)

#define END(t) \
  check(trace, #t)

static void addPass(char *trace, char *c) {
  while (*c) {
    while (*trace != '\0') {
      g_assert_true(*trace++ != '\1');
    }

    *trace++ = *c++;
  }
}

static void check(char *trace, const char *expected) {
  g_assert_cmpstr(trace, ==, expected);
}

// TE TFE TF!E
void test_case000(void) {
  START;

  try {
    PASS(t);
  } endtry;

  END(t);
}

void test_case000f(void) {
  START;

  try {
    PASS(t);
  } finally {
    PASS(f);
  } endtry;

  END(tf);
}

void test_case000F(void) {
  START;

  try {
    try {
      PASS(t);
    } finally {
      PASS(f);
      throw(newex());
    } endtry;
  } catchall {
    PASS(!);
  } endtry

  END(tf!);
}

// TCE TCFE TCF!E
void test_case010(void) {
  START;

  try {
    PASS(t);
  } catchall {
    g_test_fail();
  } endtry;

  END(t);
}

void test_case010f(void) {
  START;

  try {
    PASS(t);
  } catchall {
    g_test_fail();
  } finally {
    PASS(f);
  } endtry;

  END(tf);
}

void test_case010F(void) {
  START;

  try {
    try {
      PASS(t);
    } catchall {
      g_test_fail();
    } finally {
      PASS(f);
      throw(newex());
    } endtry;
  } catchall {
    PASS(!);
  } endtry

  END(tf!);
}

// T!E T!FE T!F!E
void test_case100(void) {
  START;

  try {
    try {
      PASS(t);
      throw(newex());
    } endtry;
  } catchall {
    PASS(!);
  } endtry

  END(t!);
}

void test_case100f(void) {
  START;

  try {
    try {
      PASS(t);
      throw(newex());
    } finally {
      PASS(f);
    } endtry;
  } catchall {
    PASS(!);
  } endtry

  END(tf!);
}

void test_case100F(void) {
  START;

  try {
    try {
      PASS(t);
      throw(newex());
    } finally {
      PASS(f);
      throw(newex());
    } endtry;
  } catchall {
    PASS(!);
  } endtry

  END(tf!);
}

// T!CE T!CFE T!CF!E
void test_case110(void) {
  START;

  try {
    PASS(t);
    throw(newex());
  } catchall {
    PASS(c);
  } endtry;

  END(tc);
}

void test_case110f(void) {
  START;

  try {
    PASS(t);
    throw(newex());
  } catchall {
    PASS(c);
  } finally {
    PASS(f);
  } endtry;

  END(tcf);
}

void test_case110F(void) {
  START;

  try {
    try {
      PASS(t);
      throw(newex());
    } catchall {
      PASS(c);
    } finally {
      PASS(f);
      throw(newex());
    } endtry;
  } catchall {
    PASS(!);
  } endtry

  END(tcf!);
}

// T!C!E T!C!FE T!C!F!E
void test_case111(void) {
  START;

  try {
    try {
      PASS(t);
      throw(newex());
    } catchall {
      PASS(c);
      throw(newex());
    } endtry;
  } catchall {
    PASS(!);
  } endtry

  END(tc!);
}

void test_case111f(void) {
  START;

  try {
    try {
      PASS(t);
      throw(newex());
    } catchall {
      PASS(c);
      throw(newex());
    } finally {
      PASS(f);
    } endtry;
  } catchall {
    PASS(!);
  } endtry

  END(tcf!);
}

void test_case111F(void) {
  START;

  try {
    try {
      PASS(t);
      throw(newex());
    } catchall {
      PASS(c);
      throw(newex());
    } finally {
      PASS(f);
      throw(newex());
    } endtry;
  } catchall {
    PASS(!);
  } endtry

  END(tcf!);
}

int main(int argc, char **argv) {
  g_test_init(&argc, &argv, NULL);

  // (f)inally, (F) with exception

  g_test_add_func("/TE/case000",      test_case000);
  g_test_add_func("/TE/case000f",     test_case000f);
  g_test_add_func("/TE/case000F",     test_case000F);

  g_test_add_func("/TCE/case010",     test_case010);
  g_test_add_func("/TCE/case010f",    test_case010f);
  g_test_add_func("/TCE/case010F",    test_case010F);

  g_test_add_func("/T!E/case100",     test_case100);
  g_test_add_func("/T!E/case100f",    test_case100f);
  g_test_add_func("/T!E/case100F",    test_case100F);

  g_test_add_func("/T!CE/case110",    test_case110);
  g_test_add_func("/T!CE/case110f",   test_case110f);
  g_test_add_func("/T!CE/case110F",   test_case110F);

  g_test_add_func("/T!C!E/case111",   test_case111);
  g_test_add_func("/T!C!E/case111f",  test_case111f);
  g_test_add_func("/T!C!E/case111F",  test_case111F);

  return g_test_run();
}

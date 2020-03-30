/* saneex.c - Pure C99 Exceptions (try/catch/finally)
   by Proger_XP | https://github.com/ProgerXP/SaneC | public domain (CC0) */

#include <stdarg.h>
#include "saneex.h"

#ifdef SX_ASSERT
#ifndef NDEBUG
#include <assert.h>
#define sxAssert(x, c) \
  assert(x)
#endif
#endif

#ifndef sxAssert
#define sxAssert(x, c) \
  if (!(x)) \
    fprintf(stderr, "saneex.c assertion error: %s (%s:%d)\n", \
      #x, __FILE__, __LINE__), \
    exit(c)
#endif

struct TryContext {
  // jmp_buf's type is an array.
  jmp_buf buf;
  int caught;
};

#define MAX_TRY_CATCH 100
static SX_THREAD_LOCAL int nextContext;
static SX_THREAD_LOCAL struct TryContext contexts[MAX_TRY_CATCH];
// _sxLastJumpCode is only meaningful for contexts[nextContext - 1].
SX_THREAD_LOCAL int _sxLastJumpCode = -1;

#define MAX_TRACE 20
static SX_THREAD_LOCAL int nextTrace;
static SX_THREAD_LOCAL struct SxTraceEntry trace[MAX_TRACE];
static SX_THREAD_LOCAL char hasUncatchable;
// Standard date/time directives are in the local TZ.
char *sxTag = __DATE__ " " __TIME__;

int sxWalkTrace(void func(const struct SxTraceEntry *, void *), void *data) {
  for (int i = 0; i < nextTrace; i++) {
    func(&trace[i], data);
  }

  return nextTrace;
}

void sxPrintEntryToStdErr(const struct SxTraceEntry *entry, void *data) {
  char ptr[20];
  int n = entry->extra == NULL ? 0 : snprintf(ptr, 20, " (%p)", entry->extra);
  ptr[n] = '\0';

  fprintf(stderr,
      "%s%s"
      "    ...%sat %s:%d, code %d"
      "%s"
      "\n",
    *entry->message == '\0' ? "" : entry->message,
    *entry->message == '\0' ? "" : "\n",
    entry->uncatchable ? "UNCATCHABLE " : "",
    entry->file, entry->line, entry->code,
    ptr
  );
}

void sxPrintTrace() {
  sxWalkTrace(sxPrintEntryToStdErr, NULL);
}

struct SxTraceEntry sxCurrentException(void) {
  struct SxTraceEntry copy;

  if (nextTrace > 0) {
    copy = trace[0];
  } else {
    copy.code = -1;
  }

  return copy;
}

char *sxlcpy(char *dest, const char *src) {
  return sxlcpyn(dest, src, SX_MAX_TRACE_STRING);
}

// Unlike strncpy(), doesn't fill remainder of dest with '\0' and puts
// '\0' at n even if str is longer than n.
// n - length including terminating '\0' (so n = 1 means "empty string").
// Returns a pointer to where terminating '\0' was written (on n == 0 - to dest).
// If src is NULL assumes it's an empty string.
char *sxlcpyn(char *dest, const char *src, int n) {
  if (n > 0) {
    while (n-- > 0 && (*dest++ = *src++)) ;
    *--dest = '\0';
  }

  return dest;
}

struct SxTraceEntry sxprintf(struct SxTraceEntry entry, const char *fmt, ...) {
  va_list arg;

  va_start(arg, fmt);
  vsnprintf(entry.message, SX_MAX_TRACE_STRING, fmt, arg);
  va_end(arg);

  return entry;
}

void sxAddTraceEntry(const struct SxTraceEntry entry) {
  // trace[0] is the first stack frame - it has initiated the exception.
  if (nextTrace < MAX_TRACE &&
      (entry.file || entry.message || entry.extra)) {
    // An uncanny feature of fixed-length char[N]'s is that the compiler won't
    // complain if it's assigned a string exactly N chars long, and it won't
    // insert '\0' (because it doesn't fit) so the string will overflow:
    //
    //   (struct SxTraceEntry) { .file = "..."; } // ... = SX_MAX_TRACE_STRING long
    //   in the end, .file looks exactly like "...", not "...\0"
    trace[nextTrace] = entry;
    trace[nextTrace].file[SX_MAX_TRACE_STRING-1] = '\0';
    trace[nextTrace].message[SX_MAX_TRACE_STRING-1] = '\0';
    nextTrace++;
  }
}

SX_NORETURN static void _throw(const struct SxTraceEntry entry) {
  sxAddTraceEntry(entry);
  hasUncatchable |= entry.uncatchable;
  const int code = entry.code;

#ifdef SX_VERBOSE
  fprintf(stderr, "% 3d _throw:    code=%d file=%s:%d msg=%s\n",
    nextContext, entry.code, entry.file, entry.line, entry.message);
#endif

  if (nextContext < 1) {
    // No wrapping try..catch block so this is an "uncaught exception".
    fprintf(stderr, "Uncaught exception (code %d) - terminating. Tag: %s\n",
      code, sxTag);
    sxPrintTrace();
    int exitCode = EXIT_UNCAUGHT + code;
    exit(exitCode > 254 ? 254 : exitCode);
  }

  longjmp( contexts[nextContext - 1].buf, code > 0 ? code : 1 );
}

// A "try" is split into two calls to _sxEnterTry/2() because:
//   "If the function which called setjmp() returns before longjmp() is called,
//    the behavior is undefined."
// try cannot be just a call to _sxEnterTry() with setjmp() inside it so
// as long as each try is paired with an endtry - it will work
// (there's no way to leave a function bypassing endtry when using re/throw).
jmp_buf *_sxEnterTry(void) {
  sxAssert(nextContext < MAX_TRY_CATCH, EXIT_MAX_TRIES);
  contexts[nextContext].caught = 0;
  return &contexts[nextContext++].buf;
}

// Returns 0 if entering a try block, non-0 if entering a catch block (i.e.
// a throw was called).
char _sxEnterTry2(int code) {
#ifdef SX_VERBOSE
  fprintf(stderr, "% 3d _sxEnterTry2: code=%d caught=%d\n", nextContext, code,
    contexts[nextContext -1].caught);
#endif

  // Used to catch bugs due to an infinite throw/try/throw/... loop.
  sxAssert(contexts[nextContext - 1].caught < 1000, EXIT_TOO_NESTED);
  return (_sxLastJumpCode = code) == 0;
}

void _sxLeaveTry(const char *file, int line) {
  sxAssert(--nextContext >= 0, EXIT_NO_TRY_ON_LEAVE);

#ifdef SX_VERBOSE
  struct TryContext *cx = &contexts[nextContext];

  fprintf(stderr, "% 3d _sxLeaveTry:  code=%d caught=%d file=%s:%d\n",
    nextContext + 1, _sxLastJumpCode, cx->caught, file, line);
#endif

  // Possible cases:
  // * (T)RY - always present
  // * (!)throw within TRY - yes/no
  // * (C)ATCH - yes/no (only considering a matching N or CATCHALL)
  // * (!)throw within CATCH - yes/no
  // * FINALLY - no (discussed layer)
  // * (E)NDTRY - always present
  //
  //   2TRY     = state at setjmp() returning != 0 first time
  //   1CATCH   = first time to CATCH (_sxSetCaught())
  //   LJC      = _sxLastJumpCode
  //   C        = caught
  //   unreach. = never called
  //
  //                2TRY        1CATCH    3TRY    2CATCH  ENDTRY
  // 000    TE      n/a         n/a       n/a     n/a     LJC=0 C=0
  //-001-  -T-!E-  (no CATCH - no throw())
  // 010    TCE     n/a         unreach.  n/a     n/a     LJC=0 C=0
  //-011-  -TC!E-  (CATCH cannot be reached without a throw() in TRY)
  // 100    T!E     LJC>0 C=0   n/a       n/a     n/a     LJC>0 C=0 >> rethrow
  //-101-  -T!-!E- (no CATCH - no throw())
  // 110    T!CE    LJC>0 C=0   LJC=0 C=1 n/a     n/a     LJC=0 C=1
  // 111    T!C!E   LJC>0 C=0   LJC=0 C=1 LJC>0 C=1} LJC>0 C=2} LJC>0 C=2 >> r.
  //
  // FINALLY reduction:
  // * Once entered sets C to a high value so even if FINALLY itself throws
  //   then neither FINALLY nor CATCH blocks are re-entered
  // * LJC isn't changed by FINALLY so that if a preceding CATCH has "unfired" an
  //   exception (as in case 110) then it's not rethrown, else (case 111) it is

  if (hasUncatchable || _sxLastJumpCode) {
    struct SxTraceEntry entry = {.code = _sxLastJumpCode, .line = line};

    snprintf(entry.message, SX_MAX_TRACE_STRING, "%srethrown by ENDTRY",
      hasUncatchable ? "UNCATCHABLE " : "");

    sxlcpy(entry.file, file);
    _throw(entry);
  }
}

// An arbitrary high value to bypass subsequent catch/catchall/finally.
#define FINALLY_THRESHOLD 50

char _sxSetCaught(char isFinally) {
  sxAssert(nextContext > 0, EXIT_OUTSIDE_CAUGHT);

  struct TryContext *cx = &contexts[nextContext - 1];
  const int caught = ++cx->caught;

  if (!isFinally) {   // a catch.
    if (caught == 1) {
      _sxLastJumpCode = 0;
      return 1;
    }
  } else {          // a finally.
    if (caught < FINALLY_THRESHOLD) {
      return cx->caught = FINALLY_THRESHOLD;
    }
  }

  return 0;
}

static void clearTrace() {
  hasUncatchable = 0;

  while (nextTrace > 0) {
    void *extra = trace[--nextTrace].extra;
    if (extra != NULL) { free(extra); }
  }
}

/*
  try {             if (setjmp() == 0) {
    throw(e);      >> line (1) > (3) > implicit rethrow()
  } endtry          } _sxLeaveTry();
*/
SX_NORETURN void sxThrow(const struct SxTraceEntry entry) {
  clearTrace();
  _throw(entry);
}

SX_NORETURN void sxRethrow(const struct SxTraceEntry entry) {
  sxAssert(nextContext > 0 &&
    // catch resets _sxLastJumpCode on enter so rethrow() will get zero.
    !_sxLastJumpCode &&
    // Detect rethrow() inside finally.
    contexts[nextContext - 1].caught < FINALLY_THRESHOLD,
    EXIT_OUTSIDE_RETHROW);

  struct SxTraceEntry entryCopy = entry;

  if (entry.code < 1) {
    entryCopy.code = _sxLastJumpCode;
  }

  _throw(entryCopy);
}

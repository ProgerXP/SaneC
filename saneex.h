/* saneex.c - Pure C99 Exceptions (try/catch/finally)
   by Proger_XP | https://github.com/ProgerXP/SaneC | public domain (CC0)
==============================================================================

    try {
      ...
    } catch(N) {         << optional (int N >= 1, an exception code)
      ...
    } catch(M) {         << ignored if M is the same as a preceding catch's N
      ...
    } catchall {         << optional, ran if no specific catch() matched
      ...
    } finally {          << optional, ran always, just once
      ...                 < exceptions here don't trigger catch or finally in
    } endtry                the same try..endtry block
______________________________________________________________________________

  Attention!
    1. Do not 'return' or 'goto' from/to anywhere between try..endtry.
       This cannot be detected even on runtime and will result in state
       desynchronization and broken windows.
    2. Do not forget endtry. The compiler will warn you about 3 unclosed '}'.
______________________________________________________________________________

  Functions available inside catch() and catchall:
    sxPrintTrace()        output current exception's trace to stderr
    sxPrintEntryToStdErr(TE, p)  output the given SxTraceEntry to stderr
    sxWalkTrace(func, p)  invoke func for all stack frames of the current ex.
    rethrow()             like throw() but preserve trace of the current ex.

  Functions available in all contexts:
    throw(SxTraceEntry)   throw an exception with additional parameters
    curex()               get current top-level trace entry, or code = -1
    thrif(x, m)           throw an exception if x holds (m = "message")
    thri(x)               like thrif() but no message
    sxprintf(TE, fmt, ...)  return a copy of TE with sprintf()'d TE.message

  SxTraceEntry struct creation macros:
    newex()               set errno, __FILE__ and __LINE__ - "NEW EXception"
    msgex(m)              ...and message (immediate value) - "MeSsaGe EXception"
    exex(m, e)            ...and extra (any pointer) - "EXtra EXception"
______________________________________________________________________________

  If an exception reaches top level without being handled, the program is
  terminated with exit() and a trace is output to stderr.

  There's no function to test if the code is running inside an exception
  handler because it may be a try..finally block without catch (so exception
  won't be caught) which on runtime cannot be told apart from a "catching"
  try..catch block.

  Exception codes can be either integer #define's or, better, enum members:

    enum {badArguments = 1, divisionByZero, userAbort} MyExceptionCodes;

    try {
      errno = badArguments;       // instead of   errno = 1;
      throw(newex());
    } catch(badArguments) {       // instead of   } catch(1) {
    } endtry
______________________________________________________________________________

  Overridable #defines:
    SX_ASSERT             use assert() instead of a check & exit() for run-time
                          state consistency checks (#define ignored if NDEBUG)
    SX_VERBOSE            output debug information to stderr
    SX_THREAD_LOCAL       type qualifier for shared variables;
                          defaults to none (not thread-safe)
    SX_NORETURN           function qualifier for compiler optimization

  Variables:
    sxTag                 is output together with a trace; defaults to
                          compilation date/time; can be e.g. a program version
______________________________________________________________________________

  Attention!

  Local variable that is not declared as volatile will become undefined
  within and after a catch block if its value was changed in a try block:

    int foo = 1;
    try {
      foo = 2;
      // here foo can be still used
    } catchall {
      // DO NOT USE foo HERE
    } finally {
      // OR HERE
    } endtry
    // OR HERE

  Compare to this safe version:

    volatile int foo = 1;
    try {
    } catchall {
      // foo can be used anywhere
    } finally {
      // even here
    } endtry
    // or here

=================================================================== saneex.c */

//#define SX_ASSERT
//#define SX_VERBOSE
//#define SX_THREAD_LOCAL _Thread_local
//#define SX_NORETURN __attribute__ ((noreturn))
//#define SX_MAX_TRACE_STRING 32
// If you have problems with default unprefixed aliases:
//#undef throw
//#define my_throw sxThrow

#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>

#ifndef SX_THREAD_LOCAL
#define SX_THREAD_LOCAL
#endif

#ifndef SX_NORETURN
#define SX_NORETURN
#endif

#ifndef SX_MAX_TRACE_STRING
#define SX_MAX_TRACE_STRING   128
#endif

// Exit codes used when saneex is terminating the process.
// In all such cases a message is output to stderr.
//
// Termination on an uncaught exception. The actual exit code will be
// EXIT_UNCAUGHT + that exception's code (minimum 1, maximum 254).
#define EXIT_UNCAUGHT         200
// Trapping the exit signal and not terminating when one of the below conditions
// (i.e. except EXIT_UNCAUGHT) has happened is a call for trouble because the
// state of saneex is messed up.
#define EXIT_MAX_TRIES        254   // too many nested try blocks.
#define EXIT_NO_TRY_ON_LEAVE  253   // endtry without a matching try.
#define EXIT_OUTSIDE_RETHROW  252   // rethrow() used outside of catch/all.
#define EXIT_OUTSIDE_CAUGHT   251   // catch/all/finally without a matching try.
#define EXIT_TOO_NESTED       250   // potentially impossible.

#define newex() \
  ((struct SxTraceEntry) {errno, 0, __FILE__, __LINE__, "\0", NULL})

#define msgex(m) \
  ((struct SxTraceEntry) {errno, 0, __FILE__, __LINE__, m, NULL})

// Example (extra will be automatically freed when this entry is evicted):
//   TimeoutException *e = malloc(sizeof(*e));
//   e->elapsed = timeElapsed;
//   e->limit = MAX_TIMEOUT;
//   throw(exex("Connection timed out", e));
#define exex(m, e) \
  ((struct SxTraceEntry) {errno, 0, __FILE__, __LINE__, m, e})

#define thri(x) \
  thrif(x, "")

#define thrif(x, m) \
  if (x) sxThrow(msgex("Assertion error: " #x "; " m))

// Output on uncaught exception.
//
//   int main(int argc, char **argv) {
//     sxTag = "For support visit http://proger.me";
extern char *sxTag;
// Used in the macros; do not use directly.
extern SX_THREAD_LOCAL int _sxLastJumpCode;

// '{{{' allows detecting a missing endtry on compile-time.
#define try           {{{ if (_sxEnterTry2( setjmp(*_sxEnterTry()) ))
#define catch(n)      else if (_sxLastJumpCode == (n) && _sxSetCaught(0))
#define catchall      else if (_sxSetCaught(0))
#define finally       if (_sxSetCaught(1))
#define endtry        _sxLeaveTry(__FILE__, __LINE__); }}}
#define curex         sxCurrentException
#define throw         sxThrow
#define rethrow       sxRethrow

struct SxTraceEntry {
  // Values below 1 are mapped to 1 (but shown verbatim in traces).
  int   code;

  // If set, exception will reach the root "no handler" and terminate the
  // program. Can be seen as a safer exit() because all "up-stack" catch/finally
  // blocks are allowed to execute and finalize things.
  char  uncatchable;

  // sxlcpy() can be used to safely copy a value from a char* to this char[]:
  //   struct SxTraceEntry entry = newex();
  //   sxlcpy(entry.message, msg);
  //
  // snprintf() is also safe for directly setting a formatted string:
  //   snprintf(entry.file, SX_MAX_TRACE_STRING, "%s foo!", s);
  //
  // A constant string can be simply assigned in the initializer:
  //   (struct SxTraceEntry) {.file = "foo.c"}
  //
  // ...but not when an expression such as ?: is used (even if it's constant):
  //   (struct SxTraceEntry) {.file = a ? "foo" : "bar"}    // compiler error
  //   (struct SxTraceEntry) {.file = file}                 // compiler error
  char  file[SX_MAX_TRACE_STRING];
  int   line;
  char  message[SX_MAX_TRACE_STRING];

  // Not used by saneex in any way except automatically free()'ing if non-NULL.
  void  *extra;
};

// Returns the number of trace entries (= the number of times func was called).
int sxWalkTrace(void func(const struct SxTraceEntry *, void *), void *data);
void sxPrintTrace();
void sxPrintEntryToStdErr(const struct SxTraceEntry *, void *);
// Returns current top-level trace entry or an entry with code = -1 if not
// executing inside a catch or finally (other fields are underfined).
struct SxTraceEntry sxCurrentException(void);
// Appends a new entry to the current trace as if an exception was thrown; no
// need to call manually. Does nothing if neither file, message or extra is set.
void sxAddTraceEntry(const struct SxTraceEntry);

// Used by the try..catch macros. Should not be called directly.
jmp_buf *_sxEnterTry(void);
char _sxEnterTry2(int code);
void _sxLeaveTry(const char *file, int line);
char _sxSetCaught(char isFinally);

// Useful for setting SxTraceEntry's file[] or message[].
// Calls sxlcpyn() with n = SX_MAX_TRACE_STRING.
char *sxlcpy(char *dest, const char *src);
// Unlike strncpy() ensures dest ends on '\0'.
char *sxlcpyn(char *dest, const char *src, int n);
//   throw(sxprintf(newex(), "errno = %d", errno));
struct SxTraceEntry sxprintf(struct SxTraceEntry entry, const char *fmt, ...);
SX_NORETURN void sxThrow(const struct SxTraceEntry);
// If code is < 1 then it's set to _sxLastJumpCode.
// If you don't want to add any info to the current exception - rethrow newex():
//     ...
//   } catchall {
//     free(buf);
//     // Not curex() because it will duplicate its message and file/line.
//     rethrow(newex());
//   }
SX_NORETURN void sxRethrow(const struct SxTraceEntry);

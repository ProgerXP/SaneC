# SaneC - A Library That Makes Programming in C Sane

This repository contains several modules that enhance the bare-bone environment provided by standard C. This code is generally:

- standalone (no external dependencies)
- portable (comformant to C99 or C11)
- in public domain ([Creative Commons Zero](https://creativecommons.org/publicdomain/zero/1.0/))

Tests are using [GLib Testing Framwork](https://wiki.gnome.org/Projects/GLib/GTester) and may be compiled as follows:

```
gcc saneex-test.c saneex.c `pkg-config --cflags --libs glib-2.0`
```


## `saneex` - Pure C99 Exceptions (`try`/`catch`/`finally`)

Example:

```
#include "saneex.h"

void sub(char fail) {
  void *p = malloc(1);
  try {
    if (fail) {
      throw(msgex("Feeling blue today..."));
    }
  } finally {
    free(p);
  } endtry
}

int main(int argc, char **argv) {
  try {
    sub(argc > 1);
  } catchall {
    rethrow(msgex("Bye-bye my little pony"));
  } endtry
}
```

Output:

```
$ gcc –o sex-demo saneex-demo.c saneex.c
$ ./sex-demo fail

Uncaught exception (code 1) - terminating.
Feeling blue today...
    ...at saneex-demo.c:7, code 0
rethrown by ENDTRY
    ...at saneex-demo.c:11, code 1
Bye-bye my little pony
    ...at saneex-demo.c:18, code 0
rethrown by ENDTRY
    ...at saneex-demo.c:19, code 1
```

### Main Features

- based on `setjmp.h`, pure C99, compiles even in Visual Studio
- nested `try` blocks, `throw()` from any point, `finally`, multiple `catch` per block (by exception code), `catchall`
- exceptions having not just code but also file/line information, message string, arbitrary pointer and the `uncatchable` flag ("soft `abort()`")
- no memory allocations or pointers (all `static`)
- optionally thread-safe with `__Thread_local` (conformant C11)

According to my [benchmark](https://habr.com/ru/post/491084/#benchres), the overhead of `setjmp()`/`longjmp()` is comparable with standard C++ exceptions. Moreover, the overhead of `setjmp()` alone (i.e. many `try` blocks, few `throw()`s) is miniscule (<5ms per 100k `try`s) - again just like with C++.

The last point is supported by [this article from 2005](https://tratt.net/laurie/blog/entries/timing_setjmp_and_the_joy_of_standards.html) where the author benchmarked `setjmp()` on OpenBSD and Solaris and found that its cost is the same as the cost of calling an empty function 1.45-2 times.

Please refer to the comment in `saneex.h` for usage details. Russian readers may also refer to [this article](https://habr.com/ru/post/491084/).

### Gotchas

1. Always end the `try` block with `endtry`. The compiler will usually warn you because `try` is opening 3 `{` and `endtry` is closing them.
2. Do not `return` from or use `goto` to jump from/to anywhere between `try` and `endtry`. This condition cannot be detected on compile-time or run-time and will have devastating effects.
3. If you are declaring a variable before `try`, modifying it inside `try` (before `catch`/`finally`) and later using it in `catch`, `finally` or after `endtry` - it must be `volatile`. The compiler will warn you.

To illustrate the last point:

```
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
```

Adding the `volatile` qualifier makes the variable usable everywhere:

```
volatile int foo = 1;
try {
} catchall {
  // foo can be used anywhere
} finally {
  // even here
} endtry
// or here
```

Note that changing a variable inside `try` and reading it afterwards is somewhat rare so `volatile` is rarely necessary (and it's making the code slower by putting the variable on stack rather than in a register).

### Alternatives

- [CException](https://sourceforge.net/projects/cexception) - very compact (60 lines), allegedly fast, nested `try`, ANSI C but no `finally` or message strings (only codes)
- [CTryCatch](https://github.com/Jamesits/CTryCatch) - compact, C99 but no nested `try`s or re-`throw()`ing inside `try`


## `saneobj` - Minimalistic Type-Safe Object System For gcc/clang

Depends on `saneex` and Plan9 extensions (`-fplan9-extensions` in GCC).

**Disclaimer:** the author does not promote OOP programming in C. `saneobj` exists for rare cases when C is perfectly fine but needs a bit more flexibility with certain tasks.

### Main Features

- uses only C's own preprocessor, no external tools
- on-heap, on-stack, global (`static`) and singleton objects
- single-class inheritance, method overrides in children (but not properties)
- class properties (non-instance, shared), abstract classes and methods
- zero-cost class-casting to a parent on compile-time
- built-in class for take/release model (reference counters)
- run-time type information (class hierarchy, names, memory sizes)
- 450 lines of code without comments
- thread-safe, `-O3` safe

### Examples

See `saneobj-demo.c` for more.

```
gcc -Wall -Wextra -fplan9-extensions saneobj-demo.c saneobj.c saneex.c
```

#### Using

Basic usage:

```
Account *account = newobj(Account);
try {
  account->balance = 456;
  account->vt->charge(123);
} finally {
  delobj(account);
} endtry
```

On-stack or `static` object:

```
Account account = {0};
newsobj(Account, account) {
  account->balance = 456;
  account->vt->charge(123);
} endsobj(account)
```

Reference counter-based ownership:

```
// Assume MyAutoref extends the built-in Autoref class.
MyAutoref *obj = newobj(MyAutoref);
obj->vt->take(obj);
obj->refs;    // 1
foo(asp(obj, Autoref));                 >------------------. [1]
delobj(obj);  // returns !0, obj is deallocated  <---.     |
                                                     |     |
void foo(Autoref *obj) {                     <-------+-----'
  obj->vt->take(obj);                                |
  obj->refs;    // 2                                 |
  ...                                                |
  delobj(obj);  // returns 0, obj->refs == 1   >-----' [2]
}
```

#### Declaring

A base class - `Fruit`:

```
struct Fruit;

// Fruit's non-instance fields (usually methods).
typedef struct {
  Object_vt_;     // add methods of the Fruit's parent class.
  void (*eat)(struct Fruit *o);   // abstract method (NULL).
  int (*caloriesPerQuantity)(struct Fruit *o, int qty);
} Fruit_vt_;

// Fruit's instance properties.
typedef struct {
  Object_;        // add properties of the Fruit's parent class.
  int calories;
} Fruit_;

// Inherit from Object - the ultimate parent class.
classdef(Fruit, Object);

// Fruit's method implementations.
int Fruit_caloriesPerQuantity(Fruit *o, int qty) {
  return o->calories * qty;
}

// Fruit's constructor.
Fruit *Fruit_new(Fruit *o, void *params) {
  if (!o->vt) { throw(msgex("Fruit is abstract!")); }
  // Call the inherited constructor.
  initnew(Fruit);
  return o;
}

// Fruit's class initializer.
Fruit_vt *vtFruit(void) {
  // Call the inherited initializer.
  linkvt(Fruit, Object) {
    // Assign method implementations for this class.
    vt.caloriesPerQuantity = Fruit_caloriesPerQuantity;
  }

  return &vt;
}
```

A sub-class - `Walnut`:

```
struct Wallnut;

// No introduced methods, just parent's.
typedef struct {
  Fruit_vt_;
} Wallnut_vt_;

// No introduced properties either.
typedef struct {
  Fruit_;
} Wallnut_;

classdef(Wallnut, Fruit);

Wallnut *Wallnut_new(Wallnut *o, void *params) {
  initnew(Wallnut);
  // Setting the initial value for an inherited property.
  o->calories = 400;
  return o;
}

// With no new class methods there's nothing to initialize so using a
// convenient short macro.
vtdef(Wallnut, Fruit) {
} endvtdef

// Using:
Wallnut *wn = newobj(Wallnut);
printf("Calories = %d\n", wn->vt->caloriesPerQuantity(asp(wn, Fruit), 5));
```

A global (`static`) singleton:

```
struct YesNo;

// Based on Autoref, a built-in class for take/release-style ownership.
typedef struct {
  Autoref_vt_;
  void (*print)(struct YesNo *o);
} YesNo_vt_;

typedef struct {
  Autoref_;
  char value;   // 1 for "yes" and 0 for "no".
} YesNo_;

classdef(YesNo, Autoref);

// Holds singleton instances.
YesNo *yesNoes[2];

// Defining the only new instance method.
void YesNo_print(YesNo *o) {
  puts(o->value ? "yes" : "NO!!!");
}

YesNo *YesNo_new(YesNo *o, void *params) {
  unsigned char value = params ? 1 : 0;

  // If a suitable singleton was already created...
  if (yesNoes[value]) {
    // Returning the singleton instance instead of the new, pre-created object.
    return yesNoes[value];
  }

  // If it wasn't created yet then take the memory provided by the caller (o).
  yesNoes[value] = o;

  initnew(YesNo);
  o->value = value;

  // Prevent this instance from being freed by incrementing the ref counter.
  // asp() is a zero-cost compile-time cast to a parent class (take() is
  // declared in the superclass - Autoref, so it expects Autoref, not YesNo).
  o->vt->take(asp(o, Autoref));

  return o;
}

YesNo_vt *vtYesNo(void) {
  linkvt(YesNo, Autoref) {
    vt.print = YesNo_print;
  }

  return &vt;
}
```

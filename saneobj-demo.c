#include <stdlib.h>
#include "saneobj.h"

// Use valgrind.

/*** Fruit - A Base Class ****************************************************/

// (*) Forward declaration so Fruit's methods can accept Fruit's instances
//     rather than void*.
struct Fruit;

typedef struct {
  Object_vt_;
  // (*) Abstract method, not defined (resolves to NULL so can't be called).
  void (*eat)(struct Fruit *o);
  int (*caloriesPerQuantity)(struct Fruit *o, int qty);
} Fruit_vt_;

typedef struct {
  Object_;
  int calories;
} Fruit_;

classdef(Fruit, Object);

int Fruit_caloriesPerQuantity(Fruit *o, int qty) {
  return o->calories * qty;
}

Fruit *Fruit_new(Fruit *o, void *params) {
  // (*) Abstract class, cannot be instantiated.
  if (!o->vt) { throw(msgex("Fruit is abstract!")); }

  initnew(Fruit);
  return o;
}

Fruit_vt *vtFruit(void) {
  linkvt(Fruit, Object) {
    vt.caloriesPerQuantity = Fruit_caloriesPerQuantity;
  }

  return &vt;
}

/*** Wallnut - A Simple Sub-Class ********************************************/

struct Wallnut;

typedef struct {
  Fruit_vt_;
  // (*) No introduced methods.
} Wallnut_vt_;

typedef struct {
  Fruit_;
  // (*) No introduced properties.
} Wallnut_;

classdef(Wallnut, Fruit);

Wallnut *Wallnut_new(Wallnut *o, void *params) {
  initnew(Wallnut);
  // (*) Setting default value for an inherited property.
  o->calories = 400;
  return o;
}

// (*) Short VT function's definition form.
vtdef(Wallnut, Fruit) {
} endvtdef

/*** Orange - A Sub-Class ****************************************************/

struct Orange;

typedef struct {
  Fruit_vt_;
  int (*cut)(struct Orange *o, int pieces);
} Orange_vt_;

typedef struct {
  Fruit_;
  char isWhole;
} Orange_;

classdef(Orange, Fruit);

void Orange_eat(Fruit *o) {
  printf("ate %s", as(o, Orange)->isWhole ? "an orange" : "a piece of orange");
}

int Orange_cut(Orange *o, int pieces) {
  if (pieces > 8) {
    throw(msgex("No, it's heurestically impossible. Get yourself a melon."));
  } else if (pieces < 0) {
    // (*) Creating a formatted message.
    throw(sxprintf(newex(),
      "If your intention is to cheat the Universe by overflowing the counter"
      " and producing %lu pieces from 1 orange I'm happy to testify that it's"
      " been made better than our machines.",
      (unsigned long) pieces));
  }

  o->isWhole = 0;
  return pieces;
}

// (*) Overriding an inherited method.
int Orange_caloriesPerQuantity(Fruit *o, int qty) {
  if (qty > 20) {
    throw(msgex("Buzzt! Buuzt! Explosion danger!"));
  }

  // (*) Calling inherited implementation and returning result.
  inherited(Orange, caloriesPerQuantity) {
    return inh->caloriesPerQuantity(o, qty);
  }

  // We know caloriesPerQuantity() is implemented in Fruit. If fails, indicates
  // a bug in inherited() implementation.
  throw(msgex("Should never happen- you know how it happens."));
}

Orange *Orange_new(Orange *o, void *params) {
  initnew(Orange);
  o->calories = 20;
  o->isWhole = 1;
  return o;
}

Orange_vt *vtOrange(void) {
  linkvt(Orange, Fruit) {
    vt.caloriesPerQuantity = Orange_caloriesPerQuantity;
    vt.eat = Orange_eat;
    vt.cut = Orange_cut;
  }

  return &vt;
}

/*** YesNo - A Parametrized Singleton ****************************************/

struct YesNo;

typedef struct {
  Autoref_vt_;
  void (*print)(struct YesNo *o);
} YesNo_vt_;

typedef struct {
  Autoref_;
  char value;
} YesNo_;

classdef(YesNo, Autoref);

YesNo *yesNoes[2];

void YesNo_print(YesNo *o) {
  puts(o->value ? "yes" : "NO!!!");
}

// value can be any sophisticated type the size of a pointer, like to a struct.
YesNo *YesNo_new(YesNo *o, void *params) {
  int value = params ? 1 : 0;

  if (yesNoes[value]) {
    // (*) Substituting input object for another one.
    return yesNoes[value];
  }

  // (*) Using memory allocated by the caller.
  yesNoes[value] = o;
  initnew(YesNo);
  * (char *) &o->value = value;
  // (*) Prevent this instance from being freed when the caller doesn't need it.
  o->vt->take(asp(o, Autoref));
  return o;
}

YesNo_vt *vtYesNo(void) {
  linkvt(YesNo, Autoref) {
    vt.print = YesNo_print;
  }

  return &vt;
}

/*** Entry Point *************************************************************/

int main () {
#ifdef SJ_TRACE_LIFE
  sjCreating = sjObjectCallbackStdErr;
  sjDeleting = sjObjectCallbackStdErr;
#endif

  // Here and below "//>" indicates the expected output.
  Orange_vt *vt = vtOrange();
  printf(
    "Orange's VT function returns %s pointers, with className == %s"
    " and parent of %s.\n",
    //> same
    vt == vtOrange() ? "same" : "different (?!)",
    //> Orange, Fruit
    vt->className, vt->parent->className);

  // (*) Instantiation.
  Fruit *orange = newobj(Orange);
  Fruit *wallnut = newobj(Wallnut);

  // (*) VT (class definition) member access. Instance property access.
  printf("We got 1 %s (%d cal.) and 1 %s (%d cal.).\n",
    //> Orange, 20
    orange->vt->className, orange->calories,
    //> Wallnut, 400
    wallnut->vt->className, wallnut->calories);

  printf(
    "%s: VT's size=%lu, object's size=%lu.\n"
    "%s: VT's size=%lu, object's size=%lu.\n",
    //> varies by platform
    orange->vt->className, orange->vt->size, orange->vt->objectSize,
    //> smaller than Orange's, smaller
    wallnut->vt->className, wallnut->vt->size, wallnut->vt->objectSize);

  // (*) Method calling, including an overriden implementation (of Orange).
  printf("10 Oranges = %d cal. 10 Wallnuts = %d cal.\n",
    //> 20*10=200
    orange->vt->caloriesPerQuantity(orange, 10),
    //> 400*10=4000
    wallnut->vt->caloriesPerQuantity(wallnut, 10));

  printf("Both objects have %s - %s.\n",
    //> the same parent
    orange->vt->parent == wallnut->vt->parent
      ? "the same parent" : "different (?!) parents",
    //> Fruit
    orange->vt->parent->className);

  // (*) Various utility functions.
  struct SjInheritedMethod eatBase =
    sjBaseMethod(aspo(orange)->vt, &orange->vt->eat);
  struct SjInheritedMethod cutBase =
    sjBaseMethod(aspo(orange)->vt, &as(orange, Orange)->vt->cut);

  printf("eat() was introduced in %s as %s, cut() - in %s as %s.\n",
    //> Fruit, abstract
    eatBase.vt->className, eatBase.method ? "concrete (?!)" : "abstract",
    //> Orange, concrete
    cutBase.vt->className, cutBase.method ? "concrete" : "abstract (?!)");

  printf("%s is compatible with %s? %s. with %s? %s. with %s? %s.\n",
    //> Orange
    orange->vt->className,
    //> Orange, yes
    vtOrange()->className,
      sjHasClass(orange, vtOrange())  ? "yes"      : "no (?!)",
    //> Object, yes
    vtObject()->className,
      sjHasClass(orange, vtObject())  ? "yes"      : "no (?!)",
    //> Wallnut, no
    vtWallnut()->className,
      sjHasClass(orange, vtWallnut()) ? "yes (?!)" : "no"
  );

  char *list1, *list2;
  printf("Full class chain: %s,\nbackwards:        %s.\n",
    //> Orange<Fruit<Object
    list1 = sjJoinClassList(orange->vt, "<", 0),
    //> Object>Fruit>Orange
    list2 = sjJoinClassList(orange->vt, ">", 1));
  free(list1);
  free(list2);

  printf("Got %d parents, 0th = %s, 1st = %s, 2nd = %s, 3rd = %p.\n",
    //> 2
    sjCountParents(orange->vt),
    //> Object
    sjNthParent(orange->vt, 0)->className,
    //> Fruit
    sjNthParent(orange->vt, 1)->className,
    //> Orange
    sjNthParent(orange->vt, 2)->className,
    //> null
    sjNthParent(orange->vt, 3));

  // (*) Class-cast to a compatible parent class. Resulting class (VT) is
  //     the same, it's just a safety check for runtime.
  printf("Let's try casting %s to Fruit/Object... got %s/%s.\n",
    //> Orange
    orange->vt->className,
    //> Orange
    as(orange, Fruit)->vt->className,
    //> Orange
    as(orange, Object)->vt->className);

  // (*) Quick compile-time class-cast.
  printf("Can do the same with asp()... got %s.\n",
    //> Orange
    asp(orange, Fruit)->vt->className);
  printf("Trying a farther parent... got %s/%s.\n",
    //> Orange
    asp(orange, Object)->vt->className,
    //> Orange
    aspo(orange)->vt->className);

  // These shouldn't actually compile - gcc needs to produce
  // "<Fruit> has no member named <Orange_>".
  //asp(orange, Orange);    // downside cast.
  //asp(orange, Wallnut);   // incompatible class (another branch).

  // (*) Class-cast in any direction (here - from base class to subclass).
  printf("Let's try casting %s to Orange... ", wallnut->vt->className);
  try {
    as(wallnut, Orange);
    printf("not good, it worked! Report this bug please!\n");
    abort();
  } catchall {
    //> Object of class Wallnut cannot be cast to Orange
    printf("uh oh, got an exception: %s\n", curex().message);
  } endtry

  printf("Trying to instantiate an abstract class of Fruit... ");
  try {
    newobj(Fruit);
    printf("not good, it worked! Report this bug please!\n");
    abort();
  } catchall {
    //> Fruit is abstract!
    printf("uh oh, got an exception: %s\n", curex().message);
    //sxPrintTrace();
  } endtry

  Orange *or = as(orange, Orange);
  printf("Cheating Orange to pieces... ");
  try {
    or->vt->cut(or, -1);
    printf("not good, it worked! Report this bug please!\n");
    abort();
  } catchall {
    //> a long piece of nonsense (cut because of the SX_MAX_TRACE_STRING limit)
    printf("uh oh, got an exception: %s\n", curex().message);
  } endtry

  //> 5 5
  printf("Cutting into %d pieces - got %d back.\n", 5, or->vt->cut(or, 5));
  //> 0
  printf("isWhole = %d.\n", or->isWhole);

  // In a real program:
  // * Objects may be created on the stack (useful for function-local objects).
  //   or be static (global or function) to never go away
  // * All code between newobj and delobj will be wrapped in try..finally.
  delobj(orange);
  delobj(wallnut);

  // Autorefs.
  // (*) Parametrized newobj.
  YesNo *no   = newobjx(YesNo, (void *) 0);
  YesNo *yes1 = newobjx(YesNo, (void *) 1);
  YesNo *yes2 = newobjx(YesNo, (void *) 2);

  printf("Got two %s %s's (yes1-2): %p == %p, and %s %s (no).\n",
    //> same, YesNo, %p, %p
    yes1 == yes2 ? "same" : "different (?!)", yes1->vt->className, yes1, yes2,
    //> different, YesNo
    yes1 == no ? "the same (?!)" : "a different", no->vt->className);

  printf("yes1 talks: ");
  //> yes
  yes1->vt->print(yes1);
  printf("no   talks: ");
  //> NO!!!
  no->vt->print(no);

  printf("yes' refs = %d, no's refs = %d.\n",
    //> 1, 1
    yes1->refs, no->refs);

  // In production code these would be called as soon as newobj returns
  // (or at the beginning of a function receiving an object argument).
  no->vt->take(asp(no, Autoref));
  yes1->vt->take(asp(yes1, Autoref));
  yes2->vt->take(asp(yes2, Autoref));
  //> 3
  printf("yes' new refs = %d.\n", yes1->refs);

  // (*) delobj returning was-deleted flag and NULL'ing the variable.
  char del1 = delobj(yes1);
  printf("Now deleting... del yes1 = %d (%p), refs = %d.\n",
    //> 0, %p, 2
    del1, yes1, yes2->refs);

  yes1 = newobjx(YesNo, (void *) 1);
  yes1->vt->take(asp(yes1, Autoref));
  printf("Got new instance of yes, %s before: %p == %p, refs = %d.\n",
    //> same
    yes1 == yes2 ? "same as" : "different (?!) from",
    //> %p, %p, 3
    yes1, yes2, yes1->refs);

  del1 = delobj(yes1);
  char del2 = delobj(yes2);
  // Don't do it at home! Only tests are allowed to del what they haven't taken.
  char delg = delobj(yesNoes[1]);
  printf("Now deleting... del yes1 = %d, yes2 = %d, global = %d (%p).\n",
    //> 0, 0, 1, NULL
    del1, del2, delg, yesNoes[1]);

  // (*) On-stack object.
  newsobj(Orange, os) {
    os->calories = 50;
    printf("Got a big orange here, a pair gives us %d cal.\n",
      //> 50*2=100
      os->vt->caloriesPerQuantity(asp(os, Fruit), 2));
  } endsobj(os)

#ifdef SJ_TRACE_LIFE
  printf(
    "The library tells us we have created %d objects and deleted %d of"
    " them (stack objects not included).\n",
    //> 4 = Orange, Wallnut, first and second unique YesNo's (yes1, no)
    sjObjectsCreated,
    //> 3 = Orange, Wallnut, first YesNo (yes1); no is still kept back in yesNoes[]
    sjObjectsDeleted);
#endif
}

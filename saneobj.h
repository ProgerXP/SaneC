/* saneobj.c - Minimalistic Type-Safe Object System For gcc/clang
==============================================================================

  This module uses Plan9 extensions (which include MSFT extensions) and so
  a compatible compiler (such as gcc or clang) with certain options (such
  as -fplan9-extensions) must be used.
______________________________________________________________________________

  Fundamental terms:

    Class
      A blueprint for creating objects. Always has a single parent class (even
      if the built-in Object). Is split into two structs named class + "_vt"
      (class properties and instance methods) and just class (instance
      properties), e.g. Object_vt and Object.

    Object (instance)
      A run-time instance of a given class - a memory block with a pointer to
      the VT (shared) and a private copy of all instance properties.

    VT (Virtual Table)
      A run-time struct holding method pointers and non-instance properties
      (shared by all objects of a given class, e.g. the class' string name).
      Acts as the class identifier on run-time (e.g. sjHasClass(obj, vtObject()))
      - a value initialized once and returned by a function named
      "vt" + class name (e.g. vtObject()).
______________________________________________________________________________

  Functions:

    sjBaseMethod(vt, vtMethod)
      Determine which class first time introduced a method

    sjHasClass(obj, vt)
      Determine if an object is compatible with some class

    sjClassList(vt, list, maxList)
      Get a list of class names (strings) implemented by an object

    sjJoinClassList(vt, joiner, parentFirst)
      Use with sjClassList() to format a class chain, e.g. Sub.Par.Object

    sjCountParents(vt)
      Determine the number of base classes (excluding the object's own class)

    sjNthParent(vt, n)
      Get a VT of a specific parent by its index (a VT "from the end")
______________________________________________________________________________

  Macros (C = class name, P = parent's class name):

    newobj(C)           - params = NULL
    newobjx(C, params)
    delobj(var)
      Instantiate and destroy objects on heap

    inherited(C, method)
      Prepare a call to the inherited implementation of method:
      inherited(obj, method) { res = inh->method(...); } - perform the call

    inhnew(C)(o, params)
    inhdel(C)(o)
      Shortcuts for calling inherited new() and del(); notes:
      1. Destructors are called in reverse order (inhnew at the function's
         start, inhdel at the end)
      2. Double brackets: second pair is calling the pointer returned by macros

    as(obj, C)
    asp(obj, C)         - "AS Parent"
    aspo(obj)           - C = Object (the ultimate parent); "AS Parent Object"
      Safe class-casts; as() is evaluated on run-time, others on compile-time
      but only when C is one of obj's parent (according to obj's declared
      type, not the real run-time value)

    setvt(C)
    initnew(C)
      Used in C_new(); initnew does both setvt & inhnew, passing-through
      params in scope (setvt/inhnew are useful separately if inherited new
      should be called at a later time, e.g. after calling some method)

    linkvt(C, P)
    vtdef(C, P)
      Used to construct C_vt()

    newsobj(C, var)           - "NEW S(tack|tatic) OBJect"
    newsobjx(C, var, params)  - "... eXtra"
    endsobj(var)
      Help instantiating on-stack objects; note: for them var is *C
______________________________________________________________________________

  Overridable #defines:

    sjAlloc(size)
    sjFree(obj)
      Default to calloc()/free(); warning: sjAlloc() must zero the memory

    SJ_TRACE_LIFE
      If defined, adds several fields to Object and tracks objects' lifetimes

    SJ_NO_EXTRA
      If defined, removes Object's extra field (saves 4 or 8 bytes per instance)

    SJ_OBJECT_MAGIC
      If defined, each object's memory starts with 4 bytes of this value

  Introduced when compiled with SJ_TRACE_LIFE #define:

    sjCreating
    sjDeleting
      Two variables with callbacks that get called during newobj and delobj

    sjObjectCallbackStub()
      A built-in function suitable as a callback that does nothing

    sjObjectCallbackStdErr()
      Another built-in for logging objects' lifecycle to stderr

    sjObjectsCreated
    sjObjectsDeleted
      Two integer variables counting newobj and delobj invocations

  Introduced when compiled with SJ_OBJECT_MAGIC #define:

    objectMagic
      All objects' memory (after vt) begins with these char[4]
______________________________________________________________________________

  New class template for copy/pasting (a commented version is below):

struct CLASS;

typedef struct {
  PARENT_vt_;
} CLASS_vt_;

typedef struct {
  PARENT_;
} CLASS_;

classdef(CLASS, PARENT);

// -- Member method definitions here --

// params can be of any pointer type.
CLASS *CLASS_new(CLASS *o, void *params) {
  initnew(CLASS);
  return o;
}

CLASS_vt *vtCLASS(void) {
  linkvt(CLASS, PARENT) {
  }

  return &vt;
}

==============================================================================

  Type names:

    * ClassName - type of this class' instance; is a compound struct of a
      pointer to ClassName_vt and of the fragment
    * ClassName_vt - "virtual table" of the class' methods
    * ClassName_ - instance fragment - all class' fields except vt; used
      together with ClassName_vt to avoid casts to obtain vt of the class
      corresponding to the variable type (which is ClassName, not ClassName_)
    * ClassName_vt_ - virtual table fragment, same purpose as ClassName_

  An illustration of why separate fragment types are necessary:

    typedef struct {
      Parent_vt vt;
      int a;
    } Parent;

    typedef struct {
      Parent;
    } Child;

    Child c;
    c->vt // has type Parent_vt, not Child_vt
    ((Child_vt *) &c->vt)->childSpecificMethod()

    // This problem is solved by defining class-specific fields in an intermediate
    // fragment type and defining a compound final type using "inclusions":
    //
    // typedef struct {
    //   Child_vt vt;
    //   ParentFrag_;
    //   ChildFrag_;
    // } Child;

  A class definition template, given class name CLASS and parent class PARENT
  (CLASS_method is a sample; all other entities must be defined):

    // Put this forward declaration if you want CLASS' methods to refer to
    // itself (e.g. as the first 'o' argument), and use 'struct CLASS' instead
    // of just 'CLASS' (in the actual definitions 'CLASS' can be still used
    // because the two types - 'struct' and no 'struct' - are compatible).
    struct CLASS;

    typedef struct {
      PARENT_vt_;
      // CLASS' methods.
      //void (*method)(struct CLASS *o);
      // WIthout the forward declaration above:
      //void (*method)(void *o);
    } CLASS_vt_;

    typedef struct {
      PARENT_vt_;
      // CLASS' properties.
    } CLASS_;

    // Note: don't use  typedef PARENT_ CLASS;  and similar for _vt as it won't
    // work (PARENT needs to be explicitly declared as an anonymous inclusion).

    classdef(CLASS, PARENT);

    // classdef() declares this function's prototype because CLASS_new()
    // needs to address it (and so does vtCLASS() to _new()).
    // As long as the function simply assigns field values, it should be immune
    // to race conditions and thus thread-safe.
    CLASS_vt *vtCLASS(void) {
      static CLASS_vt vt;

      // new is used to check if vt was already initialized; new can never be
      // empty - this method is always implemented.
      if (!vt.parent) {
        vt.parent = vtPARENT();
        vt.PARENT_vt_ = vt.parent->PARENT_vt;
        vt.size = sizeof(CLASS_vt);
        vt.objectSize = sizeof(CLASS);
        vt.className = "CLASS";
        // Type-casting these functions is necessary unless you want to accept
        // void *o and type-cast it inside the function (less convenient).
        vt.new = (ctor_t *) CLASS_new;
        //vt.del = (dtor_t *) CLASS_del;

        // -- Setting/overriding of CLASS' method pointers here --
        //vt.method = CLASS_method;

        // Methods remaining NULL are considered abstract and will error
        // if called on run-time.
      }

      return &vt;
    }

    // Alternative implementation using a convenient macro:
    CLASS_vt *vtCLASS(void) {
      linkvt(CLASS, PARENT) {
        // vt defined and PARENT_vt_, parent, size, objectSize, className, new
        // already set.
        //vt.del = (dtor_t *) CLASS_del;
        //vt.method = CLASS_method;
      }

      return &vt;
    }

    // An even shorter alternative but may break code completion because no
    // real vtCLASS method is defined until after the preprocessing phase
    // (note: the { } are optional):
    vtdef(CLASS, PARENT) {
      //vt.del = (dtor_t *) CLASS_del;
      //vt.method = CLASS_method;
    } endvtdef

    // _new normally returns the o itself but if CLASS has Autoref as one of the
    // parents _new may return another object which will be used in place of o
    // (this is similar to constructor semantics in JavaScript).
    //
    // old o must not be freed because the source of o is unknown (it can be
    // a stack or a static global), but if _new has called the inherited new on
    // old o then old o's destructor must be called before returning a different o.
    //
    // params can be of any pointer type.
    CLASS *CLASS_new(CLASS *o, void *params) {
      if (!o->vt) o->vt = vtCLASS();
      // The above line can be replaced by a convenient macro:
      setvt(CLASS);

      inherited(CLASS, new) { [result =] inh->new(o); }
      // Warning: take care not to use two different names, e.g. 'foo' and 'bar':
      //inherited(C, foo) ... inh->bar()
      // For frequently used new and del equivalent convenient macros exist:
      inhnew(CLASS)(o, params);
      // Do not use this form because if parent is also calling its parent's
      // implementations the same way then you get recursion (o->vt still
      // points to first parent's VT, so second parent is calling first
      // parent's method, not its own parent):
      //o->vt->parent->new(o);
      // It's still possible to enter a loop if CLASS' method M is calling
      // inherited method N (not M!) from PARENT and that N is calling M in
      // turn (C.M > P.N > C.M > ...). But it's rarely needed and to work around
      // simply define C.N that calls P.N even if it's all it does.

      // A class can be made abstract (non-instantiable) by putting a check
      // at the beginning of the function (before setvt/initnew):
      if (!o->vt) throw(msgex("Attempt to instantiate an abstract class."));

      // A singleton is a subtype of Autoref. There are several ways to
      // implement it; one of them is to start with this:
      if (aGlobalVarCorrespondingToThisClass) return globalVar;
      globalVar = o;
      o->vt->take(o);    // so it will never be deleted.
      // Depending on the usage pattern, it may be necessary to set globalVar
      // to NULL in CLASS_del.
      // Even without extending Autoref you can of course just throw an error:
      if (globalVar) throw(msgex("Instance already created."));

      // -- Setting CLASS' instance property values here --

      // If the instance was created using newobj() macro and an exception
      // (saneex-style longjmp()) occurs before this function returns,
      // old's o's memory will be freed but the destructor won't be called.
      // Since the caller never sees returned value, no "new o" can be returned
      // and thus it's not freed by the caller.
      //
      // Calling delobj(o) in the constructor is dangerously useless: if
      // returning a different o then double-free will happen (newobj frees old
      // o if it's different from result), if returning the o itself - it'll be
      // use-after-free; returning NULL is equivalent to an exception.
      //
      // An exception in a destructor (CLASS_del(), if defined) won't prevent
      // o's memory from being freed by the delobj() macro, but it may prevent
      // the rest of the destructor from running.

      return o;
    }

    // The  CLASS *  and  struct CLASS *  forms can be used interchangeably.
    ... CLASS_method(CLASS *o, ...) {
      // -- Implementation of the CLASS' instance method here --
      // Like in the constructor, can call inherited methods and delobj(o).
      //inherited(CLASS, method) { return int->method(o, ...); }
    }

    // Basic usage.
    CLASS *obj = newobj(CLASS);
    try {
      obj->num = 123;
      obj->vt->func(456);
    } finally {
      delobj(obj);
    } endtry

    // Likewise but on stack - faster and with automatic deallocation (but you
    // still need to call the destructor before leaving the obj's scope).
    // Attention! obj must be initialized with zeros - constructors depend on this.
    CLASS obj = {};
    // For simplicity here, Autoref objects are rejected as they may record
    // their new instances in a global state and resist deletion (singletons
    // do) by take()'ing right in constructor. newsobj macro uses
    // a different approach by only failing if obj couldn't be del()'eted
    // which allows using some Autoref objects (non-singletons) even on stack.
    assert(!sjHasClass( CLASS_new(&obj, NULL), vtAutoref() ));
    try {
      ...
    } finally {
      obj.vt->del(&obj);
    } endtry

    // On-stack helper macros creating  CLASS obj_;  and  CLASS *obj = &obj_;
    // so that obj is referneced with '->' like any on-heap object:
    newsobj(CLASS, obj) {
      ...
    } endsobj(obj)

    // Static objects don't care about deallocation and perhaps even about
    // destructors so this alone may be enough.
    CLASS obj;
    ...
    int main() {
      assert(&obj == CLASS_new(&obj, NULL));
      ...
      // If obj expects its destructor to be called (e.g. to flush buffers) then:
      } finally {
        obj.vt->del(&obj);
      } endtry
    }

    // Attention! With non-allocated objects (i.e. on stack or global) you must
    // ensure the instance's size matches the variable's size. In the examples
    // above compiler performs this check but there is a caveat:
    CommonParent job;
    someSwitch ? Sub1_new((void *) &job, NULL) : Sub2_new((void *) &job, NULL);
    // or same thing written this way:
    (someSwitch ? (ctor_t *) Sub1_new : (ctor_t *) Sub2_new)(&job, NULL);
    // The compiler allocates space for CommonParent, which may be less than
    // needed by Sub1 or Sub2! If so then memory corruption occurs. The
    // following trick may be used:
    char job_[256] = {};
    CommonParent *job = (CommonParent *) &job_;
    someSwitch ...;
    if (sizeof(job_) < job->vt->objectSize) abort();
    // Of course, it can be modified to be even safer (the code above still
    // allows job's constructor to corrupt the memory by writing to memory
    // past job_'s size). If the possible classes for job are known beforehand,
    // their VTs' objectSize can be compared with sizeof(job_) before calling
    // the constructor.
    // Or just use an on-heap object to avoid this problem altogether.

    // Multi-owned auto-managed objects using Autoref's take/release model
    // (each take() must be matched with delobj(); newobj() doesn't take()):
    MyAutoref *obj = newobj(MyAutoref);
    obj->vt->take(obj);
    obj->refs;    // 1
    foo(asp(obj, Autoref));                      >-----------. [1]
    delobj(obj);  // returns !0, obj is deallocated <--.     |
    ...                                                |     |
    void foo(Autoref *obj) {                         <-+-----'
      obj->vt->take(obj);                              |
      obj->refs;    // 2                               |
      ...                                              |
      delobj(obj);  // returns 0, obj->refs == 1   >---' [2]
    }

    // "Static" classes (with fields accessible by the class name, not instance)
    // can be created with plain C, but for the sake of completeness below is a
    // saneobj example. Fully-brown static classes with inheritance do require
    // the library; with saneobj, such a class is merely a global static object.
    struct MyStaticClass {
      int num;
      void (*method)(void);
    };

    // Forward declaration of the global variable for use in "class methods".
    struct MyStaticClass MyStaticClass;

    // A "class method".
    void MyStaticClass_method(void) {
      // "Class property".
      MyStaticClass.num++;
    }

    // An "initializer".
    struct MyStaticClass MyStaticClass = {123, MyStaticClass_method};

================================================================== saneobj.c */

//#define sjAlloc(size)       myAllocateAndZeroOut(size)
//#define sjFree(obj)         myDispose(obj)
//#define SJ_OBJECT_MAGIC     "\xBA\xAD\xBE\xEF"
//#define SJ_TRACE_LIFE
//#define SJ_NO_EXTRA

#pragma once

#include <stdatomic.h>

#ifndef SX_THREAD_LOCAL
#define SX_THREAD_LOCAL       _Thread_local
#endif
#ifndef SX_NORETURN
#define SX_NORETURN           __attribute__ ((noreturn))
#endif
#include "saneex.h"

#ifndef sjAlloc
#define sjAlloc(size)         calloc(1, size)
#endif

#ifndef sjFree
#define sjFree(obj)           free(obj)
#endif

#define C_vt_(class)          class ## _vt_
#define C_vt(class)           class ## _vt
#define C_(class)             class ## _
#define vtC(class)            vt ## class
#define C_new(class)          class ## _new
#define C_M(class, method)    class ## _ ## method

// Defines structs as both a typedef and a 'struct' because the latter
// is important for forward declarations (e.g. of an Object's classdef).
#define classdef(name, parent_a) \
  typedef struct C_vt(name) { \
    struct C_vt(parent_a)* parent; \
    C_vt_(name); \
  } C_vt(name); \
\
  typedef struct name { \
    C_vt(name) *vt; \
    C_(name); \
  } name; \
\
  C_vt(name) *vtC(name)(void)

// A complete CLASS_vt declaration (formed by the classdef() macro):
// {
//   // Pointer to a vt struct returned by vtCLASS(), NULL for Object.
//   PARENT_vt *parent;
//   ...Object's properties (size, objectSize, ...) and methods (new, del)...
//   [...other parents' methods...]
//   [[...class' own methods...]]
// }
//
// A complete CLASS declaration (formed by the classdef() macro):
// {
//   CLASS_vt *vt;
//   ...Object's properties (magic, newFile, ...)...
//   [...other parents' properties...]
//   [[...class' own properties...]]
// }

typedef void *(vt_t)(void);               // vtCLASS().
typedef void *(ctor_t)(void *, void *);   // CLASS_new().
typedef void (dtor_t)(void *);            // CLASS_del().

/*** Object - The Ultimate Root Class ****************************************/

typedef struct {
  // Size of _vt struct, not _vt_! Last method's ptr is at: size - sizeof_ptr.
  // It's used in scanning in sjInheritedMethod().
  size_t      size;
  // Size of the object's instance (created with newobj), i.e. sizeof(CLASS).
  size_t      objectSize;
  // Equals to "CLASS\0".
  const char  *className;
  ctor_t      *new;   // ConstrucTOR.
  dtor_t      *del;   // DestrucTOR.
} Object_vt_;

typedef struct {
#ifdef SJ_OBJECT_MAGIC
  // Can be used to locate instances in memory (dumps).
  char        magic[4];
#endif
#ifdef SJ_TRACE_LIFE
  // Don't modify newFile/delFile as they may point to static string consts.
  const char  *newFile;   // file/line number where the object was allocated.
  int         newLine;
  const char  *delFile;   // file/line number where it was destroyed.
  int         delLine;
#endif
#ifndef SJ_NO_EXTRA
  // Custom data not used by saneobj; is freed by Object's destructor using
  // sjFree() if non-NULL.
  void        *extra;
#endif
} Object_;

struct Object_vt;   // a forward declaration.

// Object's parent is the Object itself.
// This trick is used to avoid casting 'parent' when operating a generic
// Object_vt, i.e. to make Object's parent be of the Object_vt type.
// See sjInheritedMethod() for example.
classdef(Object, Object);

Object_vt *vtObject(void);
Object *Object_new(Object *o, void *params);
void Object_del(Object *o);

/*** Autoref - Multiple Owners (Holders), Automatic Release ******************/

struct Autoref;

typedef struct {
  Object_vt_;
  int (*take)(struct Autoref *o);
  int (*release)(struct Autoref *o);
} Autoref_vt_;

typedef struct {
  Object_;
  _Atomic int refs;
} Autoref_;

classdef(Autoref, Object);

Autoref_vt *vtAutoref(void);

// newobj() doesn't take() so after newobj() o is still "unowned" (refs = 0).
// This makes it useful for passing new instances to functions:
//
//   foo(newobj(MyAutoref));    // 0 before entering foo().
//   ...
//   void foo(Autoref *o) {
//     o->vt->take(o);   // 1
Autoref *Autoref_new(Autoref *o, void *params);
// take() and release() return the old value of o->refs.
// take() is guaranteed not to throw.
int Autoref_take(Autoref *o);
// delobj(o) only decrements the counter so use it to release & auto-delete.
// If release() returns 1 then the last ref was decremented and o is now unowned.
// If destroying manually, make sure not to free when it returns < 1 because
// another thread may be doing it.
int Autoref_release(Autoref *o);

/*** Other Macros And Functions **********************************************/

#ifdef SJ_OBJECT_MAGIC
// Exposed so that it can be read by the outside code without accessing the
// macro's value.
extern const char objectMagic[4];
#endif

typedef void SjObjectCallback(Object *obj);

void sjObjectCallbackStub(Object *obj);   // default for sjCreating/sjDeleting.
void sjObjectCallbackStdErr(Object *obj);

// The callbacks and sjObjects... are declared even without SJ_TRACE_LIFE but
// they are not used.
//
// They don't track on-stack objects.
//
// Autoref's are tracked only on allocation and deallocation and only if the same
// allocated object was returned (i.e. _new didn't substitute the return value).
//
// Individual take/release calls are not tracked.
//
// Make sure these two callbacks don't throw() and are thread-safe.
extern SjObjectCallback *sjCreating;    // called after vt->new().
extern SjObjectCallback *sjDeleting;    // called before vt->del().
extern _Atomic unsigned sjObjectsCreated;
extern _Atomic unsigned sjObjectsDeleted;

// parent is used in the 'if' instead of some other field (e.g. new) in
// hope that compiler will recognize that parent is always set (by the vt.parent
// assignment in the same condition) and possibly optimize the code.
#define linkvt(class_a, parent_a) \
  static C_vt(class_a) vt; \
\
  if (!vt.parent && ( \
        vt.parent = vtC(parent_a)(), \
        vt.C_vt_(parent_a) = vt.parent->C_vt_(parent_a), \
        vt.size = sizeof(C_vt(class_a)), \
        vt.objectSize = sizeof(class_a), \
        vt.className = #class_a, \
        vt.new = (ctor_t *) &C_new(class_a), \
        1))

#define vtdef(class, parent) \
  C_vt(class) *vtC(class)(void) { \
    linkvt(class, parent) {
//
#define endvtdef \
    } \
\
    return &vt; \
  }

#define newsobj(class, var) \
  newsobjx(class, var, NULL)

#define newsobjx(class, var, params) \
  { \
    class C_(var) = {}; \
    class *var = &C_(var); \
    if (var != C_new(class)(var, params)) { \
      sxThrow(msgex("An Autoref object (" #class ") didn't use stack memory.")); \
    } \
    try {
//
#define endsobj(var) \
    } finally { \
      if (!sjRelease(var)) { \
        sxThrow(sxprintf(newex(), \
          "A %s object created on stack cannot be released (holding Autoref?).", \
          var->vt->className)); \
      } \
      var->vt->del(var); \
    } endtry \
  }

#define newobj(class) \
  newobjx(class, NULL)

#define newobjx(class, params) \
  sjNew((ctor_t *) &C_new(class), params, sizeof(class), __FILE__, __LINE__)

void *sjNew(ctor_t *ctor, void *params, size_t size,
    const char* file, int line);

// Sets var to NULL; if need to operate on a read-only source - call sjDel().
//   if (delobj(obj)) { printf("Freed and NULL'd: %p == NULL", obj); }
//
// The "&& (..., 1)" avoids -Werror=unused-value when delobj() is not tested.
#define delobj(var) \
  ( sjDel(var, __FILE__, __LINE__) && (var = NULL, 1) )

// Determines if obj can be deleted.
// If obj is not an Autoref - returns non-zero, else calls release() and
// returns non-zero if it returns 1 (i.e. if last ref was decremented).
char sjRelease(void *obj);

// Returns non-zero when obj was freed (it doesn't always happen for Autoref's).
char sjDel(void *obj, const char* file, int line);

// Only works if you kept the conventional name 'params' for the 2nd ctor argument.
#define initnew(class) \
  setvt(class); \
  inhnew(class)(o, params)

#define setvt(class) \
  if (!o->vt) o->vt = vtC(class)()

// If getting name clashes - wrap in { } to create a new scope:
//   { inherited(C, m) { ... } }
//
// Abstract (non-implemented) methods are not called. To detect this use 'else':
//   int res; inherited(C, m) { res = inh->m(...); } else { res = -1; }
//   // or:
//   int res = -1; inherited(C, m) { res = inh->m(...); }
#define inherited(class, methodName) \
  struct SjInheritedMethod inh_ = sjInheritedMethod( \
    (Object_vt *) o->vt, &o->vt->methodName, C_M(class, methodName)); \
  C_vt(class) *inh = (C_vt(class) *) inh_.vt; \
\
  if (inh_.method)

#define inhnew(class) \
  inherited(class, new) inh->new

// For use in overridden destructors.
//
//   void MyClass_del(MyClass *o) {
//     free(o->buf);
//     inhdel(MyClass)(o);
//   }
#define inhdel(class) \
  inherited(class, del) inh->del

struct SjInheritedMethod {
  const Object_vt *vt;
  // Pointer to a pointer in vt, not to method's body (which is at *method).
  const void *const *method;
};

// vtMethod is a pointer to an entry in vt, not to the method's body.
// vtMethod must belong to vt, otherwise C's undefined behavior takes place
// (comparing pointers of different structures).
//
// *vtMethod is == methodBody if the method isn't overriden in vt's class.
//
// If the method is not implemented in any parent class beyond vtMethod
// then the returned struct's vt and method are NULL.
//
// If there was a parent class where method was declared but not implemented
// (VT's method pointer == NULL) - returned vt is non-NULL but method is NULL.
struct SjInheritedMethod sjInheritedMethod(const Object_vt *vt,
    const void *vtMethod, const void *methodBody);

// Determines where *vtMethod() was first introduced. May return NULL vt.method
// if it was intrudoced as abstract.
//
//   struct SjInheritedMethod inh = sjBaseMethod(o->vt, &o->vt->foo);
//   prints("foo() introduced in %s", inh.vt->className);
struct SjInheritedMethod sjBaseMethod(const Object_vt *vt,
    const void *vtMethod);

// Returns zero if vt (a class) is neither a part of obj's inheritance chain
// or the immediate class of obj. Otherwise returns non-zero.
int sjHasClass(const void *obj, const void *vt);

// Like sjHasClass() but returns obj or throws if it's incompatible with vt.
void *sjClassCast(void *obj, const void *vt);

// Can convert obj both to its parent and to subclasses.
// Throws if obj is not compatible with class.
#define as(obj, class) \
  ((class *) sjClassCast(obj, vtC(class)()))

// Can only convert obj to one of its parents (base classes). Unlike as(), this
// is an overhead-free yet type-safe compile-time convertion utilizing Plan9's
// struct extensions.
//
//   Somewhat *o = newobj(Somewhat);
//   Object *o2 = asp(o, Object);   // or just aspo(o).
//   o = asp(o2, Somewhat);         // won't work.
#define asp(obj, class) \
  ((void) obj->C_(class), (class *) obj)

#define aspo(obj) \
  asp(obj, Object)

// Returns number of elements in list. list = {"Sub", "Parent", ..., "Object"}.
// Do not change strings in list, they're pointing to vt: list[0][1] = "X"
int sjClassList(const Object_vt *vt, const char *list[], int maxList);

// parentFirst is a boolean.
// Call free() on the returned string (C's free(), not sjFree()).
//
//   char *s = sjJoinClassList(o->vt, " > ", 1);
//   try {
//     // s is "Object > Parent > Sub\0".
//     printf("%s", s);
//   } finally {
//     free(s);
//   } endtry
char *sjJoinClassList(const void *vt, const char *joiner, int parentFirst);

// Returns a non-negative value (count of non-NULL vt->parent's).
// The only class for which it will return 0 is Object.
int sjCountParents(const void *vt);

// Returns n'th vt from the root (Object). With n = 0 returns Object's vt;
// n = sjCountParents() returns vt itself; n > sjCountParents() returns NULL.
//
//     2        1        0    << n
//   Sub > Parent > Object    (sjCountParents() = 2)
Object_vt *sjNthParent(const void *vt, int n);

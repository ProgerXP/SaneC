/* saneobj.c - Minimalistic Type-Safe Object System For gcc/clang
   by Proger_XP | https://github.com/ProgerXP/SaneC | public domain (CC0) */

#include <stddef.h>
#include "saneobj.h"

/*** Object's methods ********************************************************/

Object_vt *vtObject(void) {
  static Object_vt vt = {NULL, {
    sizeof(Object_vt), sizeof(Object), "Object",
    (ctor_t *) Object_new, (dtor_t *) Object_del,
  }};

  return &vt;
}

Object *Object_new(Object *o, void *params) {
#ifdef SJ_OBJECT_MAGIC
  // gcc complains with -Werror=stringop-truncation.
  strncpy(o->magic, objectMagic, sizeof(o->magic) - 1);
  o->magic[sizeof(o->magic)-1] = objectMagic[sizeof(o->magic)-1];
#endif
  return o;
}

void Object_del(Object *o) {
#ifndef SJ_NO_EXTRA
  if (o->extra) {
    sjFree(o->extra);
    o->extra = NULL;
  }
#endif
}

/*** Autoref's methods *******************************************************/

Autoref_vt *vtAutoref(void) {
  linkvt(Autoref, Object) {
    vt.take = Autoref_take;
    vt.release = Autoref_release;
  }

  return &vt;
}

// In certain cases Autoref->del() may be called even when refs is non-zero.
// For example, on-stack objects don't even update refs. When ->del() is called,
// go ahead and prepare for deletion regardless of the counter's value.
Autoref *Autoref_new(Autoref *o, void *params) {
  initnew(Autoref);
  return o;
}

int Autoref_take(Autoref *o) {
  return atomic_fetch_add(&o->refs, 1);
}

int Autoref_release(Autoref *o) {
  return atomic_fetch_sub(&o->refs, 1);
}

/*** Other Functions *********************************************************/

#ifdef SJ_OBJECT_MAGIC
const char objectMagic[4] = SJ_OBJECT_MAGIC;
#endif

void sjObjectCallbackStub(Object *obj) { }

void sjObjectCallbackStdErr(Object *obj) {
#ifdef SJ_TRACE_LIFE
  enum { creating, deleting } action = obj->delFile ? deleting : creating;

  fprintf(stderr, "  [%s] %s (%s:%d)\n",
    action == creating ? "++" : "--",
    obj->vt->className,
    action == creating ? obj->newFile : obj->delFile,
    action == creating ? obj->newLine : obj->delLine
  );
#endif
}

SjObjectCallback *sjCreating = sjObjectCallbackStub;
SjObjectCallback *sjDeleting = sjObjectCallbackStub;
_Atomic unsigned sjObjectsCreated;
_Atomic unsigned sjObjectsDeleted;

static struct SxTraceEntry makeEx(const char *file, int line) {
  struct SxTraceEntry entry = {.line = line};
  sxlcpy(entry.file, file);
  return entry;
}

void *sjNew(ctor_t *ctor, void *params, size_t size,
    const char *file, int line) {
  Object *const allocated = sjAlloc(size);
  if (!allocated) {
    sxThrow(sxprintf(makeEx(file, line),
      "sjAlloc(%d) error.",
      size));
  }

  // Using volatile is necessary because the compiler can't predict that
  // try will always return normally and all longjmp()s (results of throw()
  // called from ctor and elsewhere in this function) will always pass through
  // the o=ctor(...) assignment. See man 3 longjmp, the NOTES section.
  Object *volatile o;

  try {
    o = ctor(allocated, params);

#ifdef SJ_TRACE_LIFE
    if (o == allocated) {
      o->newFile = file;
      o->newLine = line;
      sjCreating(o);
      ++sjObjectsCreated;
    }
#endif
  } catchall {
    sjFree(allocated);

    sxRethrow(sxprintf(makeEx(file, line),
      "ctor(%p) error.",
      ctor));
  } endtry

  if (o != allocated) {
    sjFree(allocated);

    if (o == NULL) {
      sxThrow(sxprintf(makeEx(file, line),
        "ctor(%p) returned NULL.",
        ctor));
    } else if (!sjHasClass(o, vtAutoref())) {
      // Not freeing o as potentially leaking memory is better than potentially
      // double-freeing it (and possibly not calling the destructor).
      sxThrow(sxprintf(makeEx(file, line),
        "ctor(%p) returned a non-input object (%s at %p) that doesn't"
        " extend Autoref.",
        ctor, o->vt->className, o));
    }
  }

  return o;
}

char sjRelease(void *obj) {
  Autoref *ar = (Autoref *) obj;
  return !sjHasClass(obj, vtAutoref()) || ar->vt->release(ar) == 1;
}

char sjDel(void *obj, const char* file, int line) {
  if (!sjRelease(obj)) {
    return 0;
  }

  try {
    Object *o = (Object *) obj;

#ifdef SJ_TRACE_LIFE
    ++sjObjectsDeleted;
    o->delFile = file;
    o->delLine = line;
    sjDeleting(o);
#endif

    o->vt->del(o);
  } finally {
    sjFree(obj);
  } endtry

  return 1;
}

struct SjInheritedMethod sjInheritedMethod(const Object_vt *vt,
    const void *vtMethod, const void *methodBody) {
  static const size_t ptrSize = sizeof(void *);
  const char *className = vt->className;
  size_t vtOffset = vtMethod - (void *) vt;

  if (vtMethod < (void *) vt || vtOffset > vt->size - ptrSize ||
      // NULL methodBody = abstract method, not sure what behavior the caller
      // expected in this case so bail out.
      !methodBody ||
      sizeof(vtOffset) < sizeof(ptrdiff_t)) {
    sxThrow(sxprintf(
      newex(),
      "sjInheritedMethod(%s): vtMethod doesn't belong to vt.",
      className));
  }

  int foundMethod = 0;
  do {
    const void *const *ptr =
      (const void *const *) (((const void *) vt) + vtOffset);

    if (foundMethod) {
      if (!*ptr) {
        // Target (found) VT has a NULL in placce of the method. By convenition,
        // this is an abstract method which isn't meant for calling, so it's
        // being treated as "no previos declaration" as if the class didn't
        // have it declared at all.
        return (struct SjInheritedMethod) {vt, NULL};
      } else if (*ptr != methodBody) {
        // When a subclass (C) doesn't override a method (of P), C's vt->method
        // points to the same place as P's vt->method:
        //   void P_method(o) { inherited(); }
        // In this example, inherited method would be the same P_method so an
        // infinite recursion will occur.
        // It's still possible for it to occur by specifically assigning the same
        // method to different points in inheritance chain as below, but this
        // is not a normal situation so it's not handled:
        //   CC vt->m == M2   C vt->m == M1   P vt->m == M2
        //   calling inherited from CC (subclass of C) will call M1 which will
        //   call M2 again because it thinks it's C's inherited method
        return (struct SjInheritedMethod) {vt, ptr};
      }
    } else if (*ptr == methodBody) {
      foundMethod = 1;
    }
  } while ((vt = vt->parent) &&
           vt->size - ptrSize >= vtOffset);

  if (foundMethod) {
    // method belongs to the first class where it's declared (not necessary
    // where it's defined). There's no parent class beyond that class that has
    // method so this is a correct termination.
    return (struct SjInheritedMethod) {NULL, NULL};
  } else {
    sxThrow(sxprintf(
      newex(),
      "sjInheritedMethod(%s): methodBody doesn't belong to any vt in the chain.",
      className));
  }
}

// A rather inefficient implementation but it's not like this function is meant
// to be used often.
struct SjInheritedMethod sjBaseMethod(const Object_vt *vt,
    const void *vtMethod) {
  void *methodBody = * (void **) vtMethod;
  struct SjInheritedMethod lastInh = {vt, methodBody};

  while (1) {
    struct SjInheritedMethod inh = sjInheritedMethod(vt, vtMethod, lastInh.method);

    if (!inh.method) {
      if (inh.vt) {    // got an abstract declaration.
        lastInh = inh;
      }
      break;
    }

    lastInh = inh;
  }

  return lastInh;
}

int sjHasClass(const void *obj, const void *vt) {
  const Object_vt *ovt = ((Object *) obj)->vt;

  do {
    if (ovt == vt) {
      return 1;
    }
  } while ((ovt = ovt->parent));

  return 0;
}

void *sjClassCast(void *obj, const void *vt) {
  if (sjHasClass(obj, vt)) {
    return obj;
  } else {
    sxThrow(sxprintf(newex(),
      "Object of class %s cannot be cast to %s.",
      ((Object *) obj)->vt->className, ((Object_vt *) vt)->className));
  }
}

int sjClassList(const Object_vt *vt, const char *list[], int maxList) {
  int i = 0;

  while (i < maxList && vt) {
    list[i++] = vt->className;
    vt = vt->parent;
  }

  return i;
}

char *sjJoinClassList(const void *vt, const char *joiner, int parentFirst) {
  static const int maxList = 64;
  const char *list[maxList];

  const int stringExtra = 4;   // for "...\0".
  const int maxString = 1000;
  char *res = malloc(maxString + stringExtra);

  try {
    int numList = sjClassList(vt, list, maxList);
    int numString = 0;
    int joinerLen = 0;

    for (int i = parentFirst ? numList - 1 : 0;
         i >= 0 && i < numList;
         i += parentFirst ? -1 : +1) {
      int len = strlen(list[i]);

      if (joinerLen + numString + len >= maxString) {
        strcpy(&res[numString], "...");
        numString += 3;
        break;
      }

      strncpy(&res[numString], joiner, joinerLen);
      numString += joinerLen;   // \0 intentionally not included.
      strcpy(&res[numString], list[i]);
      numString += len;         // \0 intentionally not included.
      joinerLen = strlen(joiner);
    }

    res[numString] = '\0';
  } catchall {
    free(res);
    sxRethrow(newex());
  } endtry

  return res;
}

int sjCountParents(const void *vt) {
  int res = -1;

  do {
    res++;
  } while ((vt = ((Object_vt *) vt)->parent));

  return res;
}

Object_vt *sjNthParent(const void *vt, int n) {
  n = sjCountParents(vt) - n;
  if (n < 0) { vt = NULL; }
  while (n-- > 0) { vt = ((Object_vt *) vt)->parent; }
  return (Object_vt *) vt;    // suppress -W: discarded const qualifier.
}

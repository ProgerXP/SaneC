/*
  This example is an implementation of Autoref using x86/64 assembly instead of
  C atomics. Note: unlike with Autoref, vt->release() must be used, not delobj().

    $ gcc main.c saneex.c saneobj.c saneobj-demo-ref.c \
        -fplan9-extensions -masm=intel -DSJ_TRACE_LIFE

    #include "saneobj-demo-ref.h"

    int main(void) {
      sjCreating = sjObjectCallbackStdErr;
      sjDeleting = sjObjectCallbackStdErr;

      ReferencedObject *o = newobj(ReferencedObject);
      o->refs;            // 0
      o->vt->take(o);     // 1
      o->vt->take(o);     // 2
      o->vt->release(o);  // 2
      o->vt->release(o);  // 1, disposed with delobj()
      o = NULL;
    }
*/

#include "saneobj.h"

struct ReferencedObject;

typedef struct {
  Object_vt_;
  int (*take)(struct ReferencedObject *o);
  // del(o) called automatically when refs hits 0 (release() returns 1).
  int (*release)(struct ReferencedObject *o);
} ReferencedObject_vt_;

typedef struct {
  Object_;
  int refs;
} ReferencedObject_;

classdef(ReferencedObject, Object);

ReferencedObject_vt *vtReferencedObject(void);
ReferencedObject *ReferencedObject_new(ReferencedObject *o, void *params);
int ReferencedObject_take(ReferencedObject *o);
int ReferencedObject_release(ReferencedObject *o);

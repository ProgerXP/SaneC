#include "saneobj-demo-ref.h"

ReferencedObject_vt *vtReferencedObject(void) {
  linkvt(ReferencedObject, Object) {
    vt.take = ReferencedObject_take;
    vt.release = ReferencedObject_release;
  }

  return &vt;
}

ReferencedObject *ReferencedObject_new(ReferencedObject *o, void *params) {
  initnew(ReferencedObject);
  return o;
}

// Usage: xadd(&refs, +1);
//
// Returns the old value of *dest.
// Due to it using Intel syntax, gcc must be given -masm=intel option or
// its assembler will error.
static int xadd(int *dest, int value) {
  asm (
    "LOCK XADD %0, %1"
    // LOCK is only applicable to instructions which have a memory
    // destination operand hence +m for *dest.
  : "+m" (*dest), "+rm" (value)
  :
  : "cc"
  );

  return value;
}

int ReferencedObject_take(ReferencedObject *o) {
  return xadd(&o->refs, +1);
}

// If returns 1, the object was freed; non-positive value may indicate an error
// or a highly concurrent environment; >1 indicates that more reference(s) are
// still held.
int ReferencedObject_release(ReferencedObject *o) {
  int refs = xadd(&o->refs, -1);

  if (refs == 1) {
    delobj(o);
  }

  return refs;
}

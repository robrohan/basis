#ifndef RUNTIME_H
#define RUNTIME_H
#include "tinylisp.h"
void printlist(lisp_state_t *s, L t);
void print(lisp_state_t *s, L x);
void gc(lisp_state_t *s);
void register_runtime_prims(lisp_state_t *s);
#endif

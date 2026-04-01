#ifndef RUNTIME_H
#define RUNTIME_H
#include "tinylisp.h"
void printlist(L t);
void print(L x);
void gc(void);
void register_runtime_prims(void);
#endif

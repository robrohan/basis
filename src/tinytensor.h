#ifndef TINYTENSOR_H
#define TINYTENSOR_H

#include "tinylisp.h"

tensor_t *alloc_tensor(I rank, const I *shape, I len, const float *data);
L tens_binop(L a, L b, char op);
void gc_tensors(void);
void register_tensor_prims(void);
int tensor_equal(const tensor_t *a, const tensor_t *b);

#endif

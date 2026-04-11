#ifndef TINYTENSOR_H
#define TINYTENSOR_H

#include "tinylisp.h"

tensor_t *alloc_tensor(lisp_state_t *s, II rank, const II *shape, II len, const float *data);
L tens_binop(lisp_state_t *s, L a, L b, char op);
void gc_tensors(lisp_state_t *s);
void register_tensor_prims(lisp_state_t *s);
int tensor_equal(const tensor_t *a, const tensor_t *b);

#endif

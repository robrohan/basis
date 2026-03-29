/* tinylisp-commented.c with NaN boxing by Robert A. van Engelen 2022 */
/* tinylisp.c but adorned with comments in an (overly) verbose C style */
#ifndef TINYLISP_H
#define TINYLISP_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PRIMS    128
#define MAX_TENSORS  256  /* max live tensors; increase as needed */
#define MAX_RANK     8    /* max tensor dimensions */

/* we only need two types to implement a Lisp interpreter:
        I    32-bit unsigned integer for tags, ordinals, heap/stack indices
        L    Lisp expression: a 64-bit IEEE 754 double used as a tagged union
             via NaN-boxing. MUST remain double — the tag bits occupy bits
             48-63 of the NaN payload, which requires 64 bits. float (32-bit)
             does not have enough NaN payload to hold both a tag and ordinal.
   I variables and function parameters are named as follows:
        i    any unsigned integer, e.g. a NaN-boxed ordinal value
        t    a NaN-boxed tag
   L variables and function parameters are named as follows:
        x,y  any Lisp expression
        n    number
        t    list
        f    function, a lambda closure or Lisp primitive
        p    pair, a cons of two Lisp expressions
        e,d  environment, a list of pairs, e.g. created with (define v x)
        v    the name of a variable (an atom) or a list of variables */
typedef uint32_t I;
typedef double   L;

/* T(x) returns the tag bits of a NaN-boxed Lisp expression x */
#define T(x) (*(uint64_t *)&(x) >> 48)

/* address of the atom heap is at the bottom of the cell stack */
#define A (char *)cell

/* number of cells for the shared stack and atom heap, increase N as desired */
#define N 1024

// These exist somewhere :)
/* tensor: rank-N array of floats, backed by r2_maths vecn_* operations */
typedef struct {
    I     rank;
    I     shape[MAX_RANK];
    I     len;   /* total number of elements (product of shape) */
    float *data; /* heap-allocated flat row-major float array   */
} tensor_t;

extern I hp, sp, th;
extern const I ATOM, PRIM, CONS, CLOS, NIL, TENS;
extern L cell[N];
extern tensor_t tensor_heap[MAX_TENSORS];
extern L nil, tru, err, env;
struct prims { const char *s; L (*f)(L, L); };
struct prims prim[MAX_PRIMS];
extern int prim_count;

L box(I t, I i);
I ord(L x);
L num(L n);
I equ(L x, L y);
L atom(const char *s);
L cons(L x, L y);
L car(L p);
L cdr(L p);
L pair(L v, L x, L e);
L closure(L v, L x, L e);
L assoc(L v, L e);
I is_nil(L x);
I let(L x);
L eval(L, L);
L evlis(L t, L e);

/////////////////////////////////////

L f_eval(L t, L e);
L f_quote(L t, L _);
L f_cons(L t, L e);
L f_car(L t, L e);
L f_cdr(L t, L e);
L f_add(L t, L e);
L f_sub(L t, L e);
L f_mul(L t, L e);
L f_div(L t, L e);
L f_int(L t, L e);
L f_lt(L t, L e);
L f_eq(L t, L e);
L f_pair(L t, L e);
L f_or(L t, L e);
L f_and(L t, L e);
L f_not(L t, L e);
L f_cond(L t, L e);
L f_if(L t, L e);
L f_leta(L t, L e);
L f_lambda(L t, L e);
L f_define(L t, L e);

// struct
// {
//     const char *s;
//     L (*f)(L, L);
// } prim[] = {{"eval", f_eval},     {"quote", f_quote},
//             {"cons", f_cons},     {"car", f_car},
//             {"cdr", f_cdr},       {"+", f_add},
//             {"-", f_sub},         {"*", f_mul},
//             {"/", f_div},         {"int", f_int},
//             {"<", f_lt},          {"eq?", f_eq},
//             {"pair?", f_pair},    {"or", f_or},
//             {"and", f_and},       {"not", f_not},
//             {"cond", f_cond},     {"if", f_if},
//             {"let*", f_leta},     {"lambda", f_lambda},
//             {"define", f_define}, {0}};

/////////////////////////////////////

L bind(L v, L t, L e);
L reduce(L f, L t, L e);
L apply(L f, L t, L e);
L eval(L x, L e);

L parse(void);
L Read(void);
L list(void);
L quote(void);
L atomic(void);

void print(L);
void printlist(L t);
void gc(void);

tensor_t *alloc_tensor(I rank, const I *shape, I len, const float *data);
L f_shape(L t, L e);
L f_rank(L t, L e);
L f_slice(L t, L e);
L f_head(L t, L e);
L f_tail(L t, L e);
L f_tensor_p(L t, L e);
L f_matmul(L t, L e);
L f_transpose(L t, L e);
L f_vabs(L t, L e);
L f_vsqrt(L t, L e);
L f_normalize(L t, L e);
L f_vpow(L t, L e);
L f_zero(L t, L e);
L f_dot(L t, L e);
L f_length(L t, L e);
L f_length2(L t, L e);
L f_dist(L t, L e);
L f_dist2(L t, L e);
L f_veq(L t, L e);

#endif /* TINYLISP_H */

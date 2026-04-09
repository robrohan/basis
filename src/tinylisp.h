/* tinylisp-commented.c with NaN boxing by Robert A. van Engelen 2022 */
/* tinylisp.c but adorned with comments in an (overly) verbose C style */
#ifndef TINYLISP_H
#define TINYLISP_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PRIMS    0x80
#define MAX_TENSORS  0x2000 /* max live tensors */
#define MAX_RANK     0x08   /* max tensor dimensions */

#define CORE_PRIM_COUNT 21  /* number of built-in core primitives (for test resets) */

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
#define T(x) (*(uint64_t *)&(x) >> 0x30)

/* address of the atom heap is at the bottom of the cell stack */
#define A (char *)cell

/* number of cells for the shared stack and atom heap */
#define N 0x10000

// These exist somewhere :)
/* tensor: rank-N array of floats, backed by r2_maths vecn_* operations */
typedef struct {
    I     rank;
    I     shape[MAX_RANK];
    I     len;   /* total number of elements (product of shape) */
    float *data; /* heap-allocated flat row-major float array   */
} tensor_t;

extern I hp, sp, th;
extern const I ATOM, PRIM, CONS, CLOS, NIL, TENS, STR;
extern L cell[N];
extern tensor_t tensor_heap[MAX_TENSORS];
extern L nil, tru, err, env;
struct prims { const char *s; L (*f)(L, L); };
extern struct prims prim[MAX_PRIMS];
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
L f_int(L t, L e);
L f_lt(L t, L e);
L f_gt(L t, L e);
L f_pair(L t, L e);
L f_or(L t, L e);
L f_and(L t, L e);
L f_not(L t, L e);
L f_cond(L t, L e);
L f_if(L t, L e);
L f_leta(L t, L e);
L f_lambda(L t, L e);
L f_define(L t, L e);
L f_set(L t, L e);
L f_defun(L t, L e);

/////////////////////////////////////

L bind(L v, L t, L e);
L reduce(L f, L t, L e);
L apply(L f, L t, L e);

extern int see;
extern FILE *input_stream; /* current input source; defaults to stdin */

char scan(void);
L parse(void);
L Read(void);
L list(void);
L quote(void);
L atomic(void);

void register_prim(const char *s, L (*f)(L, L));
void gc_core(void);

#endif /* TINYLISP_H */

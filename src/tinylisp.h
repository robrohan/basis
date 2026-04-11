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
typedef uint32_t II;
typedef double   L;

/* T(x) returns the tag bits of a NaN-boxed Lisp expression x */
#define T(x) (*(uint64_t *)&(x) >> 0x30)

/* number of cells for the shared stack and atom heap */
#define N 0x10000

/* atom, primitive, cons, closure, nil, tensor, and string tags for NaN boxing */
extern const II ATOM, PRIM, CONS, CLOS, NIL, TENS, STR;

/* These exist somewhere :) */
/* tensor: rank-N array of floats, backed by r2_maths vecn_* operations */
typedef struct {
    II     rank;
    II     shape[MAX_RANK];
    II     len;   /* total number of elements (product of shape) */
    float *data; /* heap-allocated flat row-major float array   */
} tensor_t;

/* Forward declaration — allows struct prims to reference lisp_state_t* in
   its function pointer before the full struct definition appears below. */
typedef struct lisp_state lisp_state_t;

/* Primitive function table entry: name + function pointer.
   The function receives the interpreter state, argument list, and environment. */
struct prims { const char *s; L (*f)(lisp_state_t *, L, L); };

/* All interpreter state for one Lisp instance.
   Globals from the original tinylisp.c now live here so multiple instances
   can coexist without interfering with each other. */
struct lisp_state {
    /* Lisp heap + stack (shared NaN-boxed cell array).
       Bottom portion is the atom string heap (grows up from 0).
       Top portion is the cons cell stack (grows down from N). */
    L    cell[N];
    II   hp;          /* heap pointer: next free byte in atom area */
    II   sp;          /* stack pointer: next free cell slot (grows down) */

    /* Lisp constant singletons — initialised by lisp_state_init() */
    L    l_nil, l_tru, l_err, l_env;

    /* Tokenizer state */
    char buf[256];        /* current token accumulation buffer */
    int  see;             /* lookahead character */
    FILE *input_stream;   /* current input source (stdin or a file) */

    /* Tensor heap: pool of active tensor structs; th = next free slot */
    tensor_t tensor_heap[MAX_TENSORS];
    II       th;

    /* Primitive table: populated once at init, then read-only */
    struct prims prim[MAX_PRIMS];
    int          prim_count;
};

/* address of the atom heap is at the bottom of the cell array */
#define A(s) ((char *)(s)->cell)

lisp_state_t *lisp_state_new(void);
void          lisp_state_free(lisp_state_t *s);

/* NaN-boxing helpers — pure math, no interpreter state needed */
L    box(II t, II i);
II   ord(L x);
L    num(L n);
II   equ(L x, L y);

/* Core memory operations */
L    atom(lisp_state_t *s, const char *name);
L    cons(lisp_state_t *s, L x, L y);
L    car(lisp_state_t *s, L p);
L    cdr(lisp_state_t *s, L p);
L    pair(lisp_state_t *s, L v, L x, L e);
L    closure(lisp_state_t *s, L v, L x, L e);
L    assoc(lisp_state_t *s, L v, L e);
II   is_nil(lisp_state_t *s, L x);
II   let(lisp_state_t *s, L x);
L    eval(lisp_state_t *s, L x, L e);
L    evlis(lisp_state_t *s, L t, L e);

/////////////////////////////////////

L f_eval(lisp_state_t *s, L t, L e);
L f_quote(lisp_state_t *s, L t, L _);
L f_cons(lisp_state_t *s, L t, L e);
L f_car(lisp_state_t *s, L t, L e);
L f_cdr(lisp_state_t *s, L t, L e);
L f_int(lisp_state_t *s, L t, L e);
L f_lt(lisp_state_t *s, L t, L e);
L f_gt(lisp_state_t *s, L t, L e);
L f_pair(lisp_state_t *s, L t, L e);
L f_or(lisp_state_t *s, L t, L e);
L f_and(lisp_state_t *s, L t, L e);
L f_not(lisp_state_t *s, L t, L e);
L f_cond(lisp_state_t *s, L t, L e);
L f_if(lisp_state_t *s, L t, L e);
L f_leta(lisp_state_t *s, L t, L e);
L f_lambda(lisp_state_t *s, L t, L e);
L f_define(lisp_state_t *s, L t, L e);
L f_set(lisp_state_t *s, L t, L e);
L f_defun(lisp_state_t *s, L t, L e);

/////////////////////////////////////

L bind(lisp_state_t *s, L v, L t, L e);
L reduce(lisp_state_t *s, L f, L t, L e);
L apply(lisp_state_t *s, L f, L t, L e);

/* Tokenizer / parser */
char scan(lisp_state_t *s);
L    parse(lisp_state_t *s);
L    Read(lisp_state_t *s);
L    list(lisp_state_t *s);
L    quote(lisp_state_t *s);
L    atomic(lisp_state_t *s);

void register_prim(lisp_state_t *s, const char *name, L (*f)(lisp_state_t *, L, L));
void gc_core(lisp_state_t *s);

#endif /* TINYLISP_H */

#include "tinylisp.h"
#include "tinytensor.h"
#include "runtime.h"

#include <stdio.h>

/* recursively print a sub-tensor rooted at data[offset] with given shape/rank */
static void print_tensor(const float *data, const II *shape, II rank, II offset)
{
    if (data == NULL)
    {
        return;
    }

    II i;
    printf("[");
    if (rank == 1)
    {
        for (i = 0; i < shape[0]; i++)
        {
            if (i > 0)
                printf(" ");
            printf("%.6g", data[offset + i]);
        }
    }
    else
    {
        /* stride = product of remaining dimensions */
        II stride = 1;
        for (i = 1; i < rank; i++)
            stride *= shape[i];
        for (i = 0; i < shape[0]; i++)
        {
            if (i > 0)
                printf(" ");
            print_tensor(data, shape + 1, rank - 1, offset + i * stride);
        }
    }
    printf("]");
}

/* display a Lisp list t */
void printlist(lisp_state_t *s, L t)
{
    for (putchar('(');; putchar(' '))
    {
        print(s, car(s, t));
        t = cdr(s, t);
        if (is_nil(s, t))
            break;
        if (T(t) != CONS)
        {
            printf(" ");
            print(s, t);
            break;
        }
    }
    putchar(')');
}

/* display a Lisp expression x */
void print(lisp_state_t *s, L x)
{
    if (T(x) == NIL)
        printf("()");
    else if (T(x) == ATOM)
        printf("%s", A(s) + ord(x));
    else if (T(x) == PRIM)
        printf("<%s>", s->prim[ord(x)].s);
    else if (T(x) == CONS)
        printlist(s, x);
    else if (T(x) == CLOS)
        printf("{%u}", ord(x));
    else if (T(x) == STR)
        printf("\"%s\"", A(s) + ord(x));
    else if (T(x) == TENS)
    {
        tensor_t *t = &s->tensor_heap[ord(x)];
        print_tensor(t->data, t->shape, t->rank, 0);
    }
    else
        printf("%.10lg", x);
}

/* full GC: cell stack + tensor heap */
void gc(lisp_state_t *s)
{
    gc_core(s);
    gc_tensors(s);
}

/* (print x) — display x, then newline; returns x (CL convention, usable inline) */
static L f_print(lisp_state_t *s, L t, L e)
{
    L x = eval(s, car(s, t), e);
    print(s, x);
    putchar('\n');
    return x;
}

/* (equal a b) — full equality: tensors compare rank+shape+elements, strings and
   atoms compare by interned address, numbers compare by value, everything else
   bitwise. CL `equal` does structural/deep equality. Defined here rather than
   in tinylisp.c so it can see both heaps. */
static L f_eq(lisp_state_t *s, L t, L e)
{
    t = evlis(s, t, e);
    L xa = car(s, t), xb = car(s, cdr(s, t));
    if (T(xa) == TENS && T(xb) == TENS)
        return tensor_equal(&s->tensor_heap[ord(xa)], &s->tensor_heap[ord(xb)]) ? s->l_tru : s->l_nil;
    return equ(xa, xb) ? s->l_tru : s->l_nil;
}

/* (gc) — force a garbage collection cycle, returns () */
static L f_gc(lisp_state_t *s, L t, L e)
{
    (void)t;
    (void)e;
    gc(s);
    return s->l_nil;
}


/* (load file.lisp) — evaluate all expressions in a file, then return ()
   The filename is an unquoted atom: (load test_data/assert.lisp)
   Swaps s->input_stream so nested loads and REPL resumption both work correctly. */
static L f_load(lisp_state_t *s, L t, L e)
{
    L arg = car(s, evlis(s, t, e));
    if (T(arg) != STR && T(arg) != ATOM)
        return s->l_err;

    const char *path = A(s) + ord(arg);
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "load: cannot open '%s'\n", path);
        return s->l_err;
    }

    FILE *saved_stream = s->input_stream;
    int   saved_see    = s->see;

    s->input_stream = fp;
    s->see = ' '; /* reset lookahead for new input stream */

    while (scan(s)) {
        eval(s, parse(s), s->l_env);
        gc(s);
    }

    fclose(fp);
    s->input_stream = saved_stream;
    s->see = saved_see; /* restore lookahead so caller's scanner is unaffected */
    return s->l_nil;
}

void register_runtime_prims(lisp_state_t *s)
{
    register_prim(s, "equal", f_eq);
    register_prim(s, "print", f_print);
    register_prim(s, "gc",    f_gc);
    register_prim(s, "load",  f_load);
}

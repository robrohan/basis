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
void printlist(L t)
{
    for (putchar('(');; putchar(' '))
    {
        print(car(t));
        t = cdr(t);
        if (is_nil(t))
            break;
        if (T(t) != CONS)
        {
            printf(" ");
            print(t);
            break;
        }
    }
    putchar(')');
}

/* display a Lisp expression x */
void print(L x)
{
    if (T(x) == NIL)
        printf("()");
    else if (T(x) == ATOM)
        printf("%s", A + ord(x));
    else if (T(x) == PRIM)
        printf("<%s>", prim[ord(x)].s);
    else if (T(x) == CONS)
        printlist(x);
    else if (T(x) == CLOS)
        printf("{%u}", ord(x));
    else if (T(x) == STR)
        printf("\"%s\"", A + ord(x));
    else if (T(x) == TENS)
    {
        tensor_t *t = &tensor_heap[ord(x)];
        print_tensor(t->data, t->shape, t->rank, 0);
    }
    else
        printf("%.10lg", x);
}

/* full GC: cell stack + tensor heap */
void gc(void)
{
    gc_core();
    gc_tensors();
}

/* (print x) — display x, then newline; returns x (CL convention, usable inline) */
static L f_print(L t, L e)
{
    L x = eval(car(t), e);
    print(x);
    putchar('\n');
    return x;
}

/* (equal a b) — full equality: tensors compare rank+shape+elements, strings and
   atoms compare by interned address, numbers compare by value, everything else
   bitwise. CL `equal` does sl_tructural/deep equality. Defined here rather than
   in tinylisp.c so it can see both heaps. */
static L f_eq(L t, L e)
{
    t = evlis(t, e);
    L xa = car(t), xb = car(cdr(t));
    if (T(xa) == TENS && T(xb) == TENS)
        return tensor_equal(&tensor_heap[ord(xa)], &tensor_heap[ord(xb)]) ? l_tru : l_nil;
    return equ(xa, xb) ? l_tru : l_nil;
}

/* (gc) — force a garbage collection cycle, returns () */
static L f_gc(L t, L e)
{
    (void)t;
    (void)e;
    gc();
    return l_nil;
}


/* (load file.lisp) — evaluate all expressions in a file, then return ()
   The filename is an unquoted atom: (load test_data/assert.lisp)
   Swaps input_stream so nested loads and REPL resumption both work correctly. */
static L f_load(L t, L e)
{
    L arg = car(evlis(t, e));
    if (T(arg) != STR && T(arg) != ATOM)
        return l_err;

    const char *path = A + ord(arg);
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "load: cannot open '%s'\n", path);
        return l_err;
    }

    FILE *saved_stream = input_stream;
    int   saved_see    = see;

    input_stream = fp;
    see = ' '; /* reset lookahead for new input stream */

    while (scan()) {
        eval(parse(), l_env);
        gc();
    }

    fclose(fp);
    input_stream = saved_stream;
    see = saved_see; /* restore lookahead so caller's scanner is unaffected */
    return l_nil;
}

void register_runtime_prims(void)
{
    register_prim("equal", f_eq);
    register_prim("print", f_print);
    register_prim("gc",    f_gc);
    register_prim("load",  f_load);
}

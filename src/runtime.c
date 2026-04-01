#include "tinylisp.h"
#include "tinytensor.h"
#include "runtime.h"

#include <stdio.h>

/* recursively print a sub-tensor rooted at data[offset] with given shape/rank */
static void print_tensor(const float *data, const I *shape, I rank, I offset)
{
    if (data == NULL)
    {
        return;
    }

    I i;
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
        I stride = 1;
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

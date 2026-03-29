#include "tinylisp.h"
#include <stdio.h>

/* Lisp initialization and REPL */
int main(void)
{
    I i;
    printf("tinylisp");

    nil = box(NIL, 0);
    err = atom("ERR");
    tru = atom("#t");
    env = pair(tru, tru, nil);
    for (i = 0; prim[i].s; ++i)
        env = pair(atom(prim[i].s), box(PRIM, i), env);

    while (1)
    {
        printf("\n%u> ", sp - hp / 8);
        fflush(stdout);
        print(eval(Read(), env));
        gc();
    }
}

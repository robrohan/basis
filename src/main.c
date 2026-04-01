#include "tinylisp.h"
#include "tinytensor.h"
#include "runtime.h"
#include <stdio.h>
#include <unistd.h>

static void init(void)
{
    I i;
    nil = box(NIL, 0);
    err = atom("ERR");
    tru = atom("#t");
    env = pair(tru, tru, nil);
    register_tensor_prims();
    register_runtime_prims();
    for (i = 0; prim[i].s; ++i)
        env = pair(atom(prim[i].s), box(PRIM, i), env);
}

/* headless file evaluation — no implicit output.
   expressions are evaluated for side-effects only; results are discarded.
   explicit (print x) calls will write to stdout when that primitive exists. */
static int run_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        fprintf(stderr, "basis: cannot open '%s'\n", path);
        return 1;
    }
    if (dup2(fileno(fp), fileno(stdin)) < 0)
    {
        fclose(fp);
        fprintf(stderr, "basis: cannot redirect '%s'\n", path);
        return 1;
    }
    rewind(stdin);
    while (scan())
        eval(parse(), env);
    fflush(stdout);
    fclose(fp);
    return 0;
}

/* interactive REPL — prints each result and runs GC after each expression.
   UI chrome (prompt, banner) lives here so run_file stays headless.
   future: replace with r2_termui for richer interactive experience. */
static void repl(void)
{
    printf("basis\n");
    printf("ctrl+d to quit\n");
    while (1)
    {
        printf("\n%u> ", sp - hp / 8);
        fflush(stdout);
        print(eval(Read(), env));
        gc();
    }
}

int main(int argc, char *argv[])
{
    int opt;
    char *file = NULL;

    while ((opt = getopt(argc, argv, "f:")) != -1)
    {
        switch (opt)
        {
        case 'f':
            file = optarg;
            break;
        default:
            fprintf(stderr, "usage: basis [-f file.lisp]\n");
            return 1;
        }
    }

    init();
    return file ? run_file(file) : (repl(), 0);
}

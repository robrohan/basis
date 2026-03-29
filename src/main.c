#include "tinylisp.h"
#include <stdio.h>
#include <unistd.h>

static void init(void)
{
    I i;
    nil = box(NIL, 0);
    err = atom("ERR");
    tru = atom("#t");
    env = pair(tru, tru, nil);
    for (i = 0; prim[i].s; ++i)
        env = pair(atom(prim[i].s), box(PRIM, i), env);
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
            fprintf(stderr, "usage: tensp [-f file.lisp]\n");
            return 1;
        }
    }

    init();

    if (file)
    {
        /* open the file and redirect fd 0 (stdin) to it via dup2, leaving
           the FILE* structs for stdout/stderr untouched */
        FILE *fp = fopen(file, "r");
        if (!fp)
        {
            fprintf(stderr, "tensp: cannot open '%s'\n", file);
            return 1;
        }
        if (dup2(fileno(fp), fileno(stdin)) < 0)
        {
            fclose(fp);
            fprintf(stderr, "tensp: cannot redirect '%s'\n", file);
            return 1;
        }
        /* clear any cached tty state so getchar() reads from the file fd */
        rewind(stdin);
        /* evaluate every expression in the file; print only the last result */
        L result = nil;
        while (scan())
        {
            result = eval(parse(), env);
            gc();
        }
        print(result);
        putchar('\n');
        fflush(stdout);
	fclose(fp);
        return 0;
    }

    /* REPL */
    printf("tensp");
    while (1)
    {
        printf("\n%u> ", sp - hp / 8);
        fflush(stdout);
        print(eval(Read(), env));
        gc();
    }
}

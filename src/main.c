#include "tinylisp.h"
#include "tinytensor.h"
#include "tinysymbolic.h"
#include "runtime.h"
#include "gguf_loader.h"
#include "tokenizer.h"
#include <stdio.h>
#include <unistd.h>


static void init(void)
{
    II i;
    l_nil = box(NIL, 0);
    l_err = atom("ERR");
    l_tru = atom("#t");
    l_env = pair(l_tru, l_tru, l_nil);
    register_tensor_prims();
    register_symbolic_prims();
    register_runtime_prims();
    register_gguf_prims();
    register_tokenizer_prims();
    for (i = 0; prim[i].s; ++i)
        l_env = pair(atom(prim[i].s), box(PRIM, i), l_env);
}

/* headless file evaluation — no implicit output.
   expressions are evaluated for side-effects only; results are discarded.
   explicit (print x) calls will write to stdout when that primitive exists. */
static int run_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        fprintf(stderr, "Basis: cannot open '%s'\n", path);
        return 1;
    }
    input_stream = fp;
    while (scan())
    {
        eval(parse(), l_env);
        gc();
    }
    fflush(stdout);
    fclose(fp);
    return 0;
}

/* interactive REPL — prints each result and runs GC after each expression.
   UI chrome (prompt, banner) lives here so run_file stays headless.
   future: replace with r2_termui for richer interactive experience. */
static void repl(void)
{
    printf(":: Basis version %s\n", VERSION);
    printf(":: Ctrl+d to quit\n");
    while (1)
    {
        printf("\n(%06x)[%06x]> ", sp - hp / 8, th);
        fflush(stdout);
        print(eval(Read(), l_env));
        gc();
    }
}

int main(int argc, char *argv[])
{
    int opt;
    char *file = NULL;

    while ((opt = getopt(argc, argv, "vf:")) != -1)
    {
        switch (opt)
        {
	case 'f':
	  file = optarg;
	  break;
	case 'v':
	  printf("Basis version %s\n", VERSION);
	  return 1;
	  break;
        default:
            fprintf(stderr, "Usage: basis [-v] [-f] [file.lisp]\n");
            return 1;
        }
    }
    if (!file && optind < argc)
        file = argv[optind];

    input_stream = stdin;
    init();
    return file ? run_file(file) : (repl(), 0);
}

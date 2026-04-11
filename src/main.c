#include "tinylisp.h"
#include "tinytensor.h"
#include "tinysymbolic.h"
#include "runtime.h"
#include "gguf_loader.h"
#include "tokenizer.h"
#include "repl.h"
#include <stdio.h>
#include <unistd.h>


static void init(lisp_state_t *s)
{
    II i;
    s->l_nil = box(NIL, 0);
    s->l_err = atom(s, "ERR");
    s->l_tru = atom(s, "#t");
    s->l_env = pair(s, s->l_tru, s->l_tru, s->l_nil);
    register_tensor_prims(s);
    register_symbolic_prims(s);
    register_runtime_prims(s);
    register_gguf_prims(s);
    register_tokenizer_prims(s);
    for (i = 0; s->prim[i].s; ++i)
        s->l_env = pair(s, atom(s, s->prim[i].s), box(PRIM, i), s->l_env);
}

/* headless file evaluation — no implicit output.
   expressions are evaluated for side-effects only; results are discarded.
   explicit (print x) calls will write to stdout when that primitive exists. */
static int run_file(lisp_state_t *s, const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        fprintf(stderr, "Basis: cannot open '%s'\n", path);
        return 1;
    }
    s->input_stream = fp;
    while (scan(s))
    {
        eval(s, parse(s), s->l_env);
        gc(s);
    }
    fflush(stdout);
    fclose(fp);
    return 0;
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

    lisp_state_t *s = lisp_state_new();
    s->input_stream = stdin;
    init(s);
    int ret = file ? run_file(s, file) : (repl(s), 0);
    lisp_state_free(s);
    return ret;
}

#include "tinylisp.h"
#include "tinytensor.h"
#include "tinysymbolic.h"
#include "runtime.h"
#include "gguf_loader.h"
#include "tokenizer.h"
#include "repl.h"
#include "cmd.h"
#include <stdio.h>
#include <string.h>
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
    register_repl_cmds(s);
    for (i = 0; s->prim[i].s; ++i)
        s->l_env = pair(s, atom(s, s->prim[i].s), box(PRIM, i), s->l_env);
}

/* evaluate a NUL-terminated string in the current env — shared primitive used
   by bundle loading and any future in-memory eval; fmemopen avoids touching disk */
static int run_string(lisp_state_t *s, const char *src)
{
    FILE *f = fmemopen((void *)src, strlen(src), "r");
    if (!f) return 1;
    s->input_stream = f;
    while (scan(s)) {
        eval(s, parse(s), s->l_env);
        gc(s);
    }
    fclose(f);
    s->input_stream = NULL;
    return 0;
}

/* headless file evaluation — no implicit output */
static int run_file(lisp_state_t *s, const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Basis: cannot open '%s'\n", path);
        return 1;
    }
    s->input_stream = fp;
    while (scan(s)) {
        eval(s, parse(s), s->l_env);
        gc(s);
    }
    fflush(stdout);
    fclose(fp);
    s->input_stream = NULL;
    return 0;
}


int main(int argc, char *argv[])
{
    int opt;
    char *file = NULL;
    int want_repl = 0;

    while ((opt = getopt(argc, argv, "vrf:")) != -1) {
        switch (opt) {
        case 'f':
            file = optarg;
            break;
        case 'r':
            want_repl = 1;
            break;
        case 'v':
            printf("Basis version %s\n", VERSION);
            return 0;
        default:
            fprintf(stderr, "Usage: basis [-v] [-r] [-f file.lisp] [file.lisp]\n");
            return 1;
        }
    }
    if (!file && optind < argc)
        file = argv[optind];

    /* phase 1: init */
    lisp_state_t *s = lisp_state_new();
    s->input_stream = stdin;
    init(s);

    /* phase 2: load sources */
    if (file) {
        int ret = run_file(s, file);
        if (ret) { lisp_state_free(s); return ret; }
    }

    /* phase 3: REPL — always if no file given, or explicitly requested with -r */
    if (!file || want_repl)
        repl(s);

    /* phase 4: done */
    lisp_state_free(s);
    return 0;
}

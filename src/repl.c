#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_EDITLINE
#include <editline/readline.h>
#else
/* fgets fallback for platforms without libedit — no history or line editing */
static char *readline(const char *prompt)
{
    char buf[4096];
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, sizeof(buf), stdin)) return NULL;
    buf[strcspn(buf, "\n")] = '\0';
    return strdup(buf);
}
static void add_history(const char *line) { (void)line; }
#endif

#include "r2_termui.h"
#include "tinylisp.h"
#include "runtime.h"
#include "cmd.h"
#include "repl.h"


/* Count unmatched open brackets to determine if an expression is complete.
   Skips string literals and ; line comments so they don't skew the count.
   Returns > 0 when more input is needed, 0 when balanced, < 0 if closed too far. */
static int expr_depth(const char *buf)
{
    int d = 0, in_str = 0;
    for (const char *p = buf; *p; p++) {
        if (*p == '"')              { in_str = !in_str; continue; }
        if (in_str)                 continue;
        if (*p == ';')              break;   /* line comment — skip rest */
        if (*p == '(' || *p == '[') d++;
        if (*p == ')' || *p == ']') d--;
    }
    return d;
}

void repl(lisp_state_t *s)
{
    char accum[4096];

    printf(":: Basis version %s\n", VERSION);
    printf(":: Ctrl+d to quit\n");

    while (1) {
        /* print coloured stats with printf — r2_termui macros are printf
           format strings, not embeddable string literals */
        printf(ESC_SET_ATTRIBUTE_MODE_1, 32);   /* green  */
        printf("(%06x)", s->sp - s->hp / 8);
        printf(ESC_SET_ATTRIBUTE_MODE_1, 33);   /* yellow */
        printf("[%06x]", s->th);
        printf(ESC_SET_ATTRIBUTE_MODE_1, 0);    /* reset  */
        printf("\n");
        fflush(stdout);
        char *line = readline("> ");
        if (!line) { printf("\n"); break; }   /* Ctrl+D — exit cleanly */
        if (*line == '\0') { free(line); continue; }  /* blank line — skip */

        /* slash commands: /quit, /?, etc. */
        int cmd_result = dispatch_cmd(s, line);
        if (cmd_result == CMD_QUIT) { free(line); break; }
        if (cmd_result != -1)       { free(line); continue; }

        snprintf(accum, sizeof(accum), "%s\n", line);
        add_history(line);
        free(line);

        /* accumulate continuation lines until brackets are balanced */
        while (expr_depth(accum) > 0) {
            char *more = readline("_ ");
            if (!more) break;   /* Ctrl+D mid-expression — abandon it */
            strncat(accum, more, sizeof(accum) - strlen(accum) - 2);
            strncat(accum, "\n", sizeof(accum) - strlen(accum) - 1);
            free(more);
        }

        /* feed the accumulated string to the scanner via fmemopen */
        FILE *f = fmemopen(accum, strlen(accum), "r");
        s->input_stream = f;
        s->see = ' ';
        while (scan(s)) {
            print(s, eval(s, parse(s), s->l_env));
            printf("\n");
        }
        fclose(f);
        s->input_stream = NULL;
        gc(s);
    }
}

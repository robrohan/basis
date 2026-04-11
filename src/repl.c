#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <editline/readline.h>   /* macOS system editline; Linux: install libedit-dev */

#include "r2_termui.h"
#include "tinylisp.h"
#include "runtime.h"
#include "repl.h"

/* Build a colourised prompt showing heap and tensor-heap usage.
   ESC_SET_ATTRIBUTE_MODE_1 is "\033[%dm" — a printf format string — so we
   compose the prompt incrementally, one piece per snprintf call.
   \001 / \002 are readline's prompt-ignore markers: they tell readline not to
   count the enclosed bytes when measuring the prompt width, so line-wrapping
   and cursor positioning stay correct. */
static void make_prompt(lisp_state_t *s, char *out, size_t n)
{
    int len = 0;
    len += snprintf(out + len, n - (size_t)len, "\001" ESC_SET_ATTRIBUTE_MODE_1 "\002", 32); /* green  */
    len += snprintf(out + len, n - (size_t)len, "(%06x)", s->sp - s->hp / 8);
    len += snprintf(out + len, n - (size_t)len, "\001" ESC_SET_ATTRIBUTE_MODE_1 "\002", 33); /* yellow */
    len += snprintf(out + len, n - (size_t)len, "[%06x]", s->th);
    len += snprintf(out + len, n - (size_t)len, "\001" ESC_SET_ATTRIBUTE_MODE_1 "\002", 0);  /* reset  */
          snprintf(out + len, n - (size_t)len, "> ");
}

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
    char prompt[128];
    char accum[4096];

    printf(":: Basis version %s\n", VERSION);
    printf(":: Ctrl+d to quit\n");

    while (1) {
        make_prompt(s, prompt, sizeof(prompt));
        char *line = readline(prompt);
        if (!line) { printf("\n"); break; }   /* Ctrl+D — exit cleanly */
        if (*line == '\0') { free(line); continue; }  /* blank line — skip */

        snprintf(accum, sizeof(accum), "%s\n", line);
        add_history(line);
        free(line);

        /* accumulate continuation lines until brackets are balanced */
        while (expr_depth(accum) > 0) {
            char *more = readline("  ... ");
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

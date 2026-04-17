#include "cmd.h"
#include <stdio.h>
#include <string.h>

void register_cmd(lisp_state_t *s, const char *name, const char *help, cmd_fn_t fn)
{
    if (s->cmd_count >= CMD_MAX) {
        fprintf(stderr, "cmd: table full, cannot register '%s'\n", name);
        return;
    }
    s->cmds[s->cmd_count].name = name;
    s->cmds[s->cmd_count].help = help;
    s->cmds[s->cmd_count].fn   = fn;
    s->cmd_count++;
}

/* Returns CMD_QUIT, CMD_CONTINUE, or -1 (not a slash command — caller handles it). */
int dispatch_cmd(lisp_state_t *s, const char *line)
{
    if (line[0] != '/') return -1;

    /* split "/<cmd> [args]" into cmd word and optional args */
    const char *sp   = strchr(line, ' ');
    size_t      wlen = sp ? (size_t)(sp - line) : strlen(line);
    const char *args = sp ? sp + 1 : "";

    for (int i = 0; i < s->cmd_count; i++) {
        if (strlen(s->cmds[i].name) == wlen &&
            strncmp(s->cmds[i].name, line, wlen) == 0)
            return s->cmds[i].fn(s, args);
    }

    fprintf(stderr, "Unknown command: %.*s  (type /? for help)\n", (int)wlen, line);
    return CMD_CONTINUE;
}

static int cmd_quit(lisp_state_t *s, const char *args)
{
    (void)s; (void)args;
    return CMD_QUIT;
}

static int cmd_help(lisp_state_t *s, const char *args)
{
    (void)args;
    printf("Basis %s\n", VERSION);
    printf("  Ctrl+D   exit\n");
    printf("  _        continue multi-line expression\n");
    printf("\n");
    for (int i = 0; i < s->cmd_count; i++)
        printf("  %-10s  %s\n", s->cmds[i].name, s->cmds[i].help);
    printf("\n");

    int col_w = 0;
    for (int i = 0; i < s->prim_count; i++) {
        int n = (int)strlen(s->prim[i].s);
        if (n > col_w) col_w = n;
    }
    col_w += 2;

    printf("Primitives:\n");
    for (int i = 0; i < s->prim_count; i++) {
        printf("  %-*s", col_w, s->prim[i].s);
        if ((i + 1) % 3 == 0 || i == s->prim_count - 1)
            printf("\n");
    }
    printf("\n");
    return CMD_CONTINUE;
}

void register_repl_cmds(lisp_state_t *s)
{
    register_cmd(s, "/quit", "exit the REPL",        cmd_quit);
    register_cmd(s, "/?",    "show this help screen", cmd_help);
}

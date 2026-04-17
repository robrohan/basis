#ifndef CMD_H
#define CMD_H

#include "tinylisp.h"

void register_cmd(lisp_state_t *s, const char *name, const char *help, cmd_fn_t fn);
int  dispatch_cmd(lisp_state_t *s, const char *line);
void register_repl_cmds(lisp_state_t *s);

#endif /* CMD_H */

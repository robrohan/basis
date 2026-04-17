#include "tinyregex.h"
#include "tinylisp.h"
#include "re.h"
#include <stdio.h>

/* (re-match pattern text) — find first match of pattern in text.
   Returns (start len) as a 2-element list on match, () on no match.
   start and len are byte offsets into text.
   pattern and text must be strings or atoms. */
static L f_re_match(lisp_state_t *s, L t, L e)
{
    t = evlis(s, t, e);
    L lpat = car(s, t);
    L ltxt = car(s, cdr(s, t));

    if ((T(lpat) != STR && T(lpat) != ATOM) ||
        (T(ltxt) != STR && T(ltxt) != ATOM))
        return s->l_err;

    const char *pattern = A(s) + ord(lpat);
    const char *text    = A(s) + ord(ltxt);

    int matchlen = 0;
    int start = re_match(pattern, text, &matchlen);

    if (start < 0) return s->l_nil;

    return cons(s, num(start), cons(s, num(matchlen), s->l_nil));
}

void register_regex_prims(lisp_state_t *s)
{
    register_prim(s, "re-match", f_re_match);
}

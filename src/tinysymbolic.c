#include "tinylisp.h"
#include "tinysymbolic.h"

/* match_rec: walk pat and data in lockstep, accumulating bindings.
   Variables are atoms whose name starts with '?'.
   The '?' is stripped from the key in the returned alist so callers
   can do (assoc 'x result) rather than (assoc '?x result).
   Passing s->l_err as bindings short-circuits the whole recursion. */
static L match_rec(lisp_state_t *s, L pat, L data, L bindings)
{
    if (equ(bindings, s->l_err))
        return s->l_err;

    if (T(pat) == ATOM) {
        const char *sym = A(s) + ord(pat);
        if (sym[0] == '?') {
            /* variable: look up, check consistency, or add new binding */
            L varname = atom(s, sym + 1);
            L existing = assoc(s, varname, bindings);
            if (!equ(existing, s->l_err))
                return equ(existing, data) ? bindings : s->l_err;
            return cons(s, cons(s, varname, data), bindings);
        }
        return equ(pat, data) ? bindings : s->l_err;
    }

    if (is_nil(s, pat))
        return is_nil(s, data) ? bindings : s->l_err;

    if (T(pat) == CONS) {
        if (T(data) != CONS)
            return s->l_err;
        bindings = match_rec(s, car(s, pat), car(s, data), bindings);
        return match_rec(s, cdr(s, pat), cdr(s, data), bindings);
    }

    /* numbers, tensors, strings: bitwise equality */
    return equ(pat, data) ? bindings : s->l_err;
}

/* (match pattern data)
   Returns an alist of (name . value) bindings on success,
   nil for a successful match with no variables,
   L_ERR if the pattern does not match. */
static L f_match(lisp_state_t *s, L t, L e)
{
    t = evlis(s, t, e);
    return match_rec(s, car(s, t), car(s, cdr(s, t)), s->l_nil);
}

void register_symbolic_prims(lisp_state_t *s)
{
    register_prim(s, "match", f_match);
}

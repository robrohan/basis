#include "tinylisp.h"
#include "tinysymbolic.h"

/* match_rec: walk pat and data in lockstep, accumulating bindings.
   Variables are atoms whose name starts with '?'.
   The '?' is stripped from the key in the returned alist so callers
   can do (assoc 'x result) rather than (assoc '?x result).
   Passing err as bindings short-circuits the whole recursion. */
static L match_rec(L pat, L data, L bindings)
{
    if (equ(bindings, err))
        return err;

    if (T(pat) == ATOM) {
        const char *s = A + ord(pat);
        if (s[0] == '?') {
            /* variable: look up, check consistency, or add new binding */
            L varname = atom(s + 1);
            L existing = assoc(varname, bindings);
            if (!equ(existing, err))
                return equ(existing, data) ? bindings : err;
            return cons(cons(varname, data), bindings);
        }
        return equ(pat, data) ? bindings : err;
    }

    if (is_nil(pat))
        return is_nil(data) ? bindings : err;

    if (T(pat) == CONS) {
        if (T(data) != CONS)
            return err;
        bindings = match_rec(car(pat), car(data), bindings);
        return match_rec(cdr(pat), cdr(data), bindings);
    }

    /* numbers, tensors, strings: bitwise equality */
    return equ(pat, data) ? bindings : err;
}

/* (match pattern data)
   Returns an alist of (name . value) bindings on success,
   nil for a successful match with no variables,
   ERR if the pattern does not match. */
static L f_match(L t, L e)
{
    t = evlis(t, e);
    return match_rec(car(t), car(cdr(t)), nil);
}

void register_symbolic_prims(void)
{
    register_prim("match", f_match);
}

#define R2_STRINGS_IMPLEMENTATION
#include "r2_strings.h"
#include "tinylisp.h"

/*
    atom, primitive, cons, closure, nil, tensor, and string tags for NaN boxing
    Basically, this uses the highorder bits to create types within a 64 bit
    value, and uses the lower 32 bits for the values. See box()
*/
const II ATOM = 0x7ff8, PRIM = 0x7ff9, CONS = 0x7ffa, CLOS = 0x7ffb, NIL = 0x7ffc, TENS = 0x7ffd, STR = 0x7ffe;

/* NaN-boxing specific functions:
   box(t,i): returns a new NaN-boxed double with tag t and ordinal i
   ord(x):   returns the ordinal of the NaN-boxed double x
   num(n):   convert or check number n (does nothing, e.g. could check for NaN)
   equ(x,y): returns nonzero if x equals y */
L box(II t, II i)
{
    L x = 0;
    *(uint64_t *)&x = (uint64_t)t << 0x30 | i;
    return x;
}

/* narrowed to 32 bits, removing the tag from the 64 bit number */
II ord(L x)
{
    return (II)(*(uint64_t *)&x);
}

L num(L n)
{
    return n;
}

II equ(L x, L y)
{
    return *(uint64_t *)&x == *(uint64_t *)&y;
}

/* interning of atom names (Lisp symbols), returns a unique NaN-boxed ATOM */
L atom(lisp_state_t *s, const char *name)
{
    II i = 0;
    while (i < s->hp && strcmp(A(s) + i, name)) /* search for a matching atom name on the heap */
        i += strlen(A(s) + i) + 1;
    if (i == s->hp)
    {                                             /* if not found */
        s->hp += strlen(strcpy(A(s) + i, name)) + 1; /*   allocate and add a new atom name to the heap */
        if (s->hp > s->sp << 3)                   /* abort when out of memory */
            abort();
    }
    return box(ATOM, i);
}

/* construct pair (x . y) returns a NaN-boxed CONS */
L cons(lisp_state_t *s, L x, L y)
{
    s->cell[--s->sp] = x;   /* push the car value x */
    s->cell[--s->sp] = y;   /* push the cdr value y */
    if (s->hp > s->sp << 3) /* abort when out of memory */
        abort();
    return box(CONS, s->sp);
}

/* return the car of a pair or ERR if not a pair */
L car(lisp_state_t *s, L p)
{
    return (T(p) & ~(CONS ^ CLOS)) == CONS ? s->cell[ord(p) + 1] : s->l_err;
}

/* return the cdr of a pair or ERR if not a pair */
L cdr(lisp_state_t *s, L p)
{
    return (T(p) & ~(CONS ^ CLOS)) == CONS ? s->cell[ord(p)] : s->l_err;
}

/* construct a pair to add to environment e, returns the list ((v . x) . e) */
L pair(lisp_state_t *s, L v, L x, L e)
{
    return cons(s, cons(s, v, x), e);
}

/* construct a lambda closure with variables v body x environment e, returns a NaN-boxed CLOS */
L closure(lisp_state_t *s, L v, L x, L e)
{
    return box(CLOS, ord(pair(s, v, x, equ(e, s->l_env) ? s->l_nil : e)));
}

/* look up a symbol v in environment e, return its value or ERR if not found */
L assoc(lisp_state_t *s, L v, L e)
{
    while (T(e) == CONS && !equ(v, car(s, car(s, e))))
        e = cdr(s, e);
    return T(e) == CONS ? cdr(s, car(s, e)) : s->l_err;
}

/* is_nil(x) is nonzero if x is the Lisp () empty list a.k.a. nil or false */
II is_nil(lisp_state_t *s, L x)
{
    (void)s;
    return T(x) == NIL;
}

/* let(x) is nonzero if x has more than one item, used by let* */
II let(lisp_state_t *s, L x)
{
    return !is_nil(s, x) && !is_nil(s, cdr(s, x));
}

/* return a new list of evaluated Lisp expressions t in environment e */
L eval(lisp_state_t *s, L x, L e);
L evlis(lisp_state_t *s, L t, L e)
{
    return T(t) == CONS ? cons(s, eval(s, car(s, t), e), evlis(s, cdr(s, t), e))
         : T(t) == ATOM ? assoc(s, t, e)
         : s->l_nil; /* NOLINT */
}

/* Lisp primitives:
   (eval x)                return evaluated x (such as when x was quoted)
   (quote x)               special form, returns x unevaluated "as is"
   (cons x y)              construct pair (x . y)
   (car p)                 car of pair p
   (cdr p)                 cdr of pair p
   (truncate n)            integer part of n (CL truncate)
   (< n1 n2)               #t if n1<n2, otherwise ()
   (> n1 n2)               #t if n1>n2, otherwise ()
   (consp x)               #t if x is a non-empty list, a cons cell or closure
   (or x1 x2 ... xk)       first x that is not (), otherwise ()
   (and x1 x2 ... xk)      last x if all x are not (), otherwise ()
   (not x)                 #t if x is (), otherwise ()
   (cond (x1 y1)
         (x2 y2)
         ...
         (xk yk))          the first yi for which xi evaluates to non-()
   (if x y z)              if x is non-() then y else z
   (let* (v1 x1)
         (v2 x2)
         ...
         y)                sequentially binds each variable v1 to xi to evaluate y
   (lambda v x)            construct a closure
   (define v x)            define a named value globally (kept for backward compat)
   (defparameter v x)      define a named value globally (CL style)
   (defvar v x)            define a named value globally (CL style)
   (defun name (args) body) define a named function globally (CL style)
   (setq v x)              update an existing binding in-place (CL setq) */
L f_eval(lisp_state_t *s, L t, L e)
{
    return eval(s, car(s, evlis(s, t, e)), e);
}

L f_quote(lisp_state_t *s, L t, L _)
{
    (void)_;
    return car(s, t);
}

L f_cons(lisp_state_t *s, L t, L e)
{
    t = evlis(s, t, e);
    return cons(s, car(s, t), car(s, cdr(s, t)));
}

L f_car(lisp_state_t *s, L t, L e)
{
    return car(s, car(s, evlis(s, t, e)));
}

L f_cdr(lisp_state_t *s, L t, L e)
{
    return cdr(s, car(s, evlis(s, t, e)));
}

L f_int(lisp_state_t *s, L t, L e)
{
    L n = car(s, evlis(s, t, e));
    return n < 1e16 && n > -1e16 ? (long long)n : n;
}

L f_lt(lisp_state_t *s, L t, L e)
{
    t = evlis(s, t, e);
    return car(s, t) - car(s, cdr(s, t)) < 0 ? s->l_tru : s->l_nil;
}

L f_gt(lisp_state_t *s, L t, L e)
{
    t = evlis(s, t, e);
    return car(s, t) - car(s, cdr(s, t)) > 0 ? s->l_tru : s->l_nil;
}

L f_pair(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    return T(x) == CONS ? s->l_tru : s->l_nil;
}

L f_or(lisp_state_t *s, L t, L e)
{
    L x = s->l_nil;
    while (!is_nil(s, t) && is_nil(s, x = eval(s, car(s, t), e)))
        t = cdr(s, t);
    return x;
}

L f_and(lisp_state_t *s, L t, L e)
{
    L x = s->l_tru;
    while (!is_nil(s, t) && !is_nil(s, x = eval(s, car(s, t), e)))
        t = cdr(s, t);
    return x;
}

L f_not(lisp_state_t *s, L t, L e)
{
    return is_nil(s, car(s, evlis(s, t, e))) ? s->l_tru : s->l_nil;
}

L f_cond(lisp_state_t *s, L t, L e)
{
    while (is_nil(s, eval(s, car(s, car(s, t)), e)))
        t = cdr(s, t);
    return eval(s, car(s, cdr(s, car(s, t))), e);
}

L f_if(lisp_state_t *s, L t, L e)
{
    return eval(s, car(s, cdr(s, is_nil(s, eval(s, car(s, t), e)) ? cdr(s, t) : t)), e);
}

L f_leta(lisp_state_t *s, L t, L e)
{
    for (; let(s, t); t = cdr(s, t))
        e = pair(s, car(s, car(s, t)), eval(s, car(s, cdr(s, car(s, t))), e), e);
    return eval(s, car(s, t), e);
}

L f_lambda(lisp_state_t *s, L t, L e)
{
    return closure(s, car(s, t), car(s, cdr(s, t)), e);
}

L f_define(lisp_state_t *s, L t, L e)
{
    s->l_env = pair(s, car(s, t), eval(s, car(s, cdr(s, t)), e), s->l_env);
    return car(s, t);
}

/* (setq v x) — update the first binding of v in env in-place; error if unbound */
L f_set(lisp_state_t *s, L t, L e)
{
    L var = car(s, t);
    L val = eval(s, car(s, cdr(s, t)), e);
    L p = s->l_env;
    while (T(p) == CONS) {
        L binding = car(s, p);
        if (equ(car(s, binding), var)) {
            s->cell[ord(binding)] = val; /* overwrite cdr of (v . old) in place */
            return val;
        }
        p = cdr(s, p);
    }
    return s->l_err; /* variable not found */
}

/* (defun name (args) body) — define a named function, CL style */
L f_defun(lisp_state_t *s, L t, L e)
{
    L name = car(s, t);
    L args = car(s, cdr(s, t));
    L body = car(s, cdr(s, cdr(s, t)));
    s->l_env = pair(s, name, closure(s, args, body, e), s->l_env);
    return name;
}

/* table of Lisp core primitives, each has a name and function pointer.
   This is a template used to initialise lisp_state_t->prim[] at startup. */
static const struct prims core_prims[CORE_PRIM_COUNT + 1] = {
    {"eval",         f_eval},
    {"quote",        f_quote},
    {"cons",         f_cons},
    {"car",          f_car},
    {"cdr",          f_cdr},
    {"truncate",     f_int},
    {"<",            f_lt},
    {">",            f_gt},
    {"consp",        f_pair},
    {"or",           f_or},
    {"and",          f_and},
    {"not",          f_not},
    {"cond",         f_cond},
    {"if",           f_if},
    {"let*",         f_leta},
    {"lambda",       f_lambda},
    {"define",       f_define},
    {"defparameter", f_define},
    {"defvar",       f_define},
    {"defun",        f_defun},
    {"setq",         f_set},
    {0, 0}
};

void register_prim(lisp_state_t *s, const char *name, L (*f)(lisp_state_t *, L, L))
{
    s->prim[s->prim_count].s = name;
    s->prim[s->prim_count].f = f;
    s->prim_count++;
    /* keep the sentinel intact */
    s->prim[s->prim_count].s = 0;
}

/* create environment by extending e with variables v bound to values t */
L bind(lisp_state_t *s, L v, L t, L e)
{
    return is_nil(s, v) ? e
         : T(v) == CONS ? bind(s, cdr(s, v), cdr(s, t), pair(s, car(s, v), car(s, t), e))
         : pair(s, v, t, e);
}

/* apply closure f to arguments t in environment e */
L reduce(lisp_state_t *s, L f, L t, L e)
{
    return eval(s, cdr(s, car(s, f)),
                bind(s, car(s, car(s, f)), evlis(s, t, e),
                     is_nil(s, cdr(s, f)) ? s->l_env : cdr(s, f)));
}

/* apply closure or primitive f to arguments t in environment e, or return ERR */
L apply(lisp_state_t *s, L f, L t, L e)
{
    return T(f) == PRIM ? s->prim[ord(f)].f(s, t, e)
         : T(f) == CLOS ? reduce(s, f, t, e)
         : s->l_err;
}

/* evaluate x and return its value in environment e */
L eval(lisp_state_t *s, L x, L e)
{
    return T(x) == ATOM ? assoc(s, x, e)
         : T(x) == CONS ? apply(s, eval(s, car(s, x), e), cdr(s, x), e)
         : x;
}

/* advance to the next character */
static void look(lisp_state_t *s)
{
    int c = getc(s->input_stream);
    s->see = c;
}

/* return nonzero if we are looking at character c, ' ' means any white space */
static II seeing(lisp_state_t *s, int c)
{
    return c == ' ' ? (s->see > 0 && s->see <= c) : s->see == c;
}

/* return the look ahead character from standard input, advance to the next */
static int get(lisp_state_t *s)
{
    int c = s->see;
    look(s);
    return c;
}

/* tokenize into buf[], return first character of buf[] */
char scan(lisp_state_t *s)
{
    II i = 0;
    while (seeing(s, ' ') || seeing(s, ';') || seeing(s, '#'))
    {
        if (seeing(s, ';'))
            while (s->see >= 0 && !seeing(s, '\n')) /* skip to end of line */
                look(s);
        else if (seeing(s, '#'))
        {
            look(s); /* consume '#', peek at next char */
            if (s->see == '!')  /* shebang line — skip to end of line */
                while (s->see >= 0 && !seeing(s, '\n'))
                    look(s);
            else
            {
                s->buf[i++] = '#'; /* '#t', '#f', etc — put '#' in buf and break out so
                                      the normal token reader appends the rest (e.g. 't') */
                break;
            }
        }
        else
            look(s);
    }
    if (s->see < 0)
    {
        s->buf[0] = 0;
        return 0;
    } /* EOF */
    if (seeing(s, '(') || seeing(s, ')') || seeing(s, '\'') || seeing(s, '[') || seeing(s, ']'))
        s->buf[i++] = (char)get(s);
    else if (seeing(s, '"'))
    {
        s->buf[i++] = (char)get(s); /* store opening " as marker for atomic() */
        while (s->see != '"' && s->see >= 0 && i < 254)
        {
            int bytes = utf8_len((char)s->see);
            int b;
            for (b = 0; b < bytes && i < 254; b++)
                s->buf[i++] = (char)get(s);
        }
        if (s->see == '"')
            look(s); /* consume closing " without storing it */
    }
    else
        do
        {
            /* collect all bytes of the current UTF-8 character before checking
               for token boundaries. utf8_len returns 1 for plain ASCII. */
            int bytes = utf8_len((char)s->see);
            int b;
            for (b = 0; b < bytes && i < 255; b++)
                s->buf[i++] = (char)get(s);
        } while (i < 255 && s->see >= 0 && !seeing(s, '(') && !seeing(s, ')') && !seeing(s, '[') && !seeing(s, ']') && !seeing(s, ' '));
    s->buf[i] = 0;
    return *s->buf;
}

/* return the Lisp expression read from standard input */
L Read(lisp_state_t *s)
{
    if (!scan(s))
        exit(0); /* EOF — exit REPL */
    return parse(s);
}

/* return a parsed Lisp list */
L list(lisp_state_t *s)
{
    L x;
    if (scan(s) == ')')
        return s->l_nil;
    if (!strcmp(s->buf, "."))
    {
        x = Read(s);
        scan(s);
        return x;
    }
    x = parse(s);
    return cons(s, x, list(s));
}

/* return a parsed Lisp expression x quoted as (quote x) */
L quote(lisp_state_t *s)
{
    return cons(s, atom(s, "quote"), cons(s, Read(s), s->l_nil));
}

/* return a parsed atomic Lisp expression (a number, atom, or string literal) */
L atomic(lisp_state_t *s)
{
    L n;
    II i;
    if (s->buf[0] == '"')
    {
        /* intern raw bytes (without the leading ") into the atom heap, tag as STR */
        L a = atom(s, s->buf + 1);
        return box(STR, ord(a));
    }
    return (sscanf(s->buf, "%lg%n", &n, &i) > 0 && !s->buf[i]) ? n : atom(s, s->buf);
}

/* collect elements of a tensor literal until ']', recursing into parse()
   so that any expression -- atom, number, (s-expr), or nested [tensor] --
   is accepted as an element */
static L tensor_elems(lisp_state_t *s)
{
    L x;
    if (scan(s) == ']')
        return s->l_nil;
    x = parse(s);
    return cons(s, x, tensor_elems(s));
}

/* parse [ e1 e2 ... ] into a (make-tensor e1 e2 ...) CONS form.
   evaluation happens later, so sub-expressions like (+ 3 x) work fine. */
static L tensor_lit(lisp_state_t *s)
{
    return cons(s, atom(s, "make-tensor"), tensor_elems(s));
}

/* return a parsed Lisp expression */
L parse(lisp_state_t *s)
{
    return *s->buf == '(' ? list(s)
         : *s->buf == '[' ? tensor_lit(s)
         : *s->buf == '\'' ? quote(s)
         : atomic(s);
}

/* gc_core: discard temporary cells, keeping only the global environment */
void gc_core(lisp_state_t *s)
{
    s->sp = ord(s->l_env);
}

/* lisp_state_new: allocate and zero-initialise a fresh interpreter state.
   Callers must call lisp_state_init() (via main.c's init()) before use. */
lisp_state_t *lisp_state_new(void)
{
    lisp_state_t *s = (lisp_state_t *)calloc(1, sizeof(lisp_state_t));
    if (!s) abort();
    s->hp  = 0;
    s->sp  = N;
    s->th  = 0;
    s->see = ' ';
    /* copy core primitive table into instance */
    int i;
    for (i = 0; i <= CORE_PRIM_COUNT; i++)
        s->prim[i] = core_prims[i];
    s->prim_count = CORE_PRIM_COUNT;
    return s;
}

/* lisp_state_free: release all tensor data and the state struct itself */
void lisp_state_free(lisp_state_t *s)
{
    II i;
    for (i = 0; i < s->th; i++)
    {
        free(s->tensor_heap[i].data);
        s->tensor_heap[i].data = NULL;
    }
    free(s);
}

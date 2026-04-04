#define R2_STRINGS_IMPLEMENTATION
#include "r2_strings.h"
#include "tinylisp.h"

/* hp: top of the atom heap pointer, A+hp with hp=0 points to the first atom string in cell[]
   sp: cell stack pointer, the stack starts at the top of cell[] with sp=N
   safety invariant: hp <= sp<<3 */
I hp = 0, sp = N;

/*
    atom, primitive, cons, closure, nil, and tensor tags for NaN boxing
    Basically, this uses the highorder bits to create types within a 64 bit
    value, and uses the lower 32 bits for the values. See box()
*/
const I ATOM = 0x7ff8, PRIM = 0x7ff9, CONS = 0x7ffa, CLOS = 0x7ffb, NIL = 0x7ffc, TENS = 0x7ffd, STR = 0x7ffe;

/* tensor heap: pool of tensor_t structs; th is the next free slot.
   tensor data arrays are malloc'd and freed by gc_tensors() */
tensor_t tensor_heap[MAX_TENSORS];
I th = 0;

/* cell[N] array of Lisp expressions, shared by the stack and atom heap */
L cell[N];

/* Lisp constant expressions () (nil), #t, ERR, and the global environment env */
L nil, tru, err, env;

/* NaN-boxing specific functions:
   box(t,i): returns a new NaN-boxed double with tag t and ordinal i
   ord(x):   returns the ordinal of the NaN-boxed double x
   num(n):   convert or check number n (does nothing, e.g. could check for NaN)
   equ(x,y): returns nonzero if x equals y */
L box(I t, I i)
{
    L x = 0;
    *(uint64_t *)&x = (uint64_t)t << 0x30 | i;
    return x;
}

/* narrowed to 32 bits, removing the tag from the 64 bit number */
I ord(L x)
{
    return (I)(*(uint64_t *)&x);
}

L num(L n)
{
    return n;
}

I equ(L x, L y)
{
    return *(uint64_t *)&x == *(uint64_t *)&y;
}

/* interning of atom names (Lisp symbols), returns a unique NaN-boxed ATOM */
L atom(const char *s)
{
    I i = 0;
    while (i < hp && strcmp(A + i, s)) /* search for a matching atom name on the heap */
        i += strlen(A + i) + 1;
    if (i == hp)
    {                                       /* if not found */
        hp += strlen(strcpy(A + i, s)) + 1; /*   allocate and add a new atom name to the heap */
        if (hp > sp << 3)                   /* abort when out of memory */
            abort();
    }
    return box(ATOM, i);
}

/* construct pair (x . y) returns a NaN-boxed CONS */
L cons(L x, L y)
{
    cell[--sp] = x;   /* push the car value x */
    cell[--sp] = y;   /* push the cdr value y */
    if (hp > sp << 3) /* abort when out of memory */
        abort();
    return box(CONS, sp);
}

/* return the car of a pair or ERR if not a pair */
L car(L p)
{
    return (T(p) & ~(CONS ^ CLOS)) == CONS ? cell[ord(p) + 1] : err;
}

/* return the cdr of a pair or ERR if not a pair */
L cdr(L p)
{
    return (T(p) & ~(CONS ^ CLOS)) == CONS ? cell[ord(p)] : err;
}

/* construct a pair to add to environment e, returns the list ((v . x) . e) */
L pair(L v, L x, L e)
{
    return cons(cons(v, x), e);
}

/* construct a lambda closure with variables v body x environment e, returns a NaN-boxed CLOS */
L closure(L v, L x, L e)
{
    return box(CLOS, ord(pair(v, x, equ(e, env) ? nil : e)));
}

/* look up a symbol v in environment e, return its value or ERR if not found */
L assoc(L v, L e)
{
    while (T(e) == CONS && !equ(v, car(car(e))))
        e = cdr(e);
    return T(e) == CONS ? cdr(car(e)) : err;
}

/* is_nil(x) is nonzero if x is the Lisp () empty list a.k.a. nil or false */
I is_nil(L x)
{
    return T(x) == NIL;
}

/* let(x) is nonzero if x has more than one item, used by let* */
I let(L x)
{
    return !is_nil(x) && !is_nil(cdr(x));
}

/* return a new list of evaluated Lisp expressions t in environment e */
L eval(L, L);
L evlis(L t, L e)
{
    return T(t) == CONS ? cons(eval(car(t), e), evlis(cdr(t), e)) : T(t) == ATOM ? assoc(t, e) : nil; /* NOLINT */
}

/* Lisp primitives:
   (eval x)            return evaluated x (such as when x was quoted)
   (quote x)           special form, returns x unevaluated "as is"
   (cons x y)          construct pair (x . y)
   (car p)             car of pair p
   (cdr p)             cdr of pair p
   (int n)             integer part of n
   (< n1 n2)           #t if n1<n2, otherwise ()
   (eq? x y)           #t if x equals y, otherwise ()
   (pair? x)           #t if x is a non-empty list, a cons cell or closure
   (or x1 x2 ... xk)   first x that is not (), otherwise ()
   (and x1 x2 ... xk)  last x if all x are not (), otherwise ()
   (not x)             #t if x is (), otherwise ()
   (cond (x1 y1)
         (x2 y2)
         ...
         (xk yk))      the first yi for which xi evaluates to non-()
   (if x y z)          if x is non-() then y else z
   (let* (v1 x1)
         (v2 x2)
         ...
         y)            sequentially binds each variable v1 to xi to evaluate y
   (lambda v x)        construct a closure
   (define v x)        define a named value globally */
L f_eval(L t, L e)
{
    return eval(car(evlis(t, e)), e);
}

L f_quote(L t, L _)
{
    return car(t);
}

L f_cons(L t, L e)
{
    t = evlis(t, e);
    return cons(car(t), car(cdr(t)));
}

L f_car(L t, L e)
{
    return car(car(evlis(t, e)));
}

L f_cdr(L t, L e)
{
    return cdr(car(evlis(t, e)));
}

L f_int(L t, L e)
{
    L n = car(evlis(t, e));
    return n < 1e16 && n > -1e16 ? (long long)n : n;
}

L f_lt(L t, L e)
{
    return t = evlis(t, e), car(t) - car(cdr(t)) < 0 ? tru : nil;
}

L f_eq(L t, L e)
{
    return t = evlis(t, e), equ(car(t), car(cdr(t))) ? tru : nil;
}

L f_pair(L t, L e)
{
    L x = car(evlis(t, e));
    return T(x) == CONS ? tru : nil;
}

L f_or(L t, L e)
{
    L x = nil;
    while (!is_nil(t) && is_nil(x = eval(car(t), e)))
        t = cdr(t);
    return x;
}

L f_and(L t, L e)
{
    L x = tru;
    while (!is_nil(t) && !is_nil(x = eval(car(t), e)))
        t = cdr(t);
    return x;
}

L f_not(L t, L e)
{
    return is_nil(car(evlis(t, e))) ? tru : nil;
}

L f_cond(L t, L e)
{
    while (is_nil(eval(car(car(t)), e)))
        t = cdr(t);
    return eval(car(cdr(car(t))), e);
}

L f_if(L t, L e)
{
    return eval(car(cdr(is_nil(eval(car(t), e)) ? cdr(t) : t)), e);
}

L f_leta(L t, L e)
{
    for (; let(t); t = cdr(t))
        e = pair(car(car(t)), eval(car(cdr(car(t))), e), e);
    return eval(car(t), e);
}

L f_lambda(L t, L e)
{
    return closure(car(t), car(cdr(t)), e);
}

L f_define(L t, L e)
{
    env = pair(car(t), eval(car(cdr(t)), e), env);
    return car(t);
}

/* (set! v x) — update the first binding of v in env in-place; error if unbound */
L f_set(L t, L e)
{
    L var = car(t);
    L val = eval(car(cdr(t)), e);
    L p = env;
    while (T(p) == CONS) {
        L binding = car(p);
        if (equ(car(binding), var)) {
            cell[ord(binding)] = val; /* overwrite cdr of (v . old) in place */
            return val;
        }
        p = cdr(p);
    }
    return err; /* variable not found */
}

/* table of Lisp core primitives, each has a name s and function pointer f */
struct prims prim[MAX_PRIMS] = {
    {"eval",    f_eval},
    {"quote",   f_quote},
    {"cons",    f_cons},
    {"car",     f_car},
    {"cdr",     f_cdr},
    {"int",     f_int},
    {"<",       f_lt},
    {"eq?",     f_eq},
    {"pair?",   f_pair},
    {"or",      f_or},
    {"and",     f_and},
    {"not",     f_not},
    {"cond",    f_cond},
    {"if",      f_if},
    {"let*",    f_leta},
    {"lambda",  f_lambda},
    {"define",  f_define},
    {"def",     f_define},
    {"set!",    f_set},
    {0}
};
int prim_count = CORE_PRIM_COUNT;

void register_prim(const char *s, L (*f)(L, L))
{
    prim[prim_count].s = s;
    prim[prim_count].f = f;
    prim_count++;
    /* keep the sentinel intact */
    prim[prim_count].s = 0;
}

/* create environment by extending e with variables v bound to values t */
L bind(L v, L t, L e)
{
    return is_nil(v) ? e : T(v) == CONS ? bind(cdr(v), cdr(t), pair(car(v), car(t), e)) : pair(v, t, e);
}

/* apply closure f to arguments t in environemt e */
L reduce(L f, L t, L e)
{
    return eval(cdr(car(f)), bind(car(car(f)), evlis(t, e), is_nil(cdr(f)) ? env : cdr(f)));
}

/* apply closure or primitive f to arguments t in environment e, or return ERR */
L apply(L f, L t, L e)
{
    return T(f) == PRIM ? prim[ord(f)].f(t, e) : T(f) == CLOS ? reduce(f, t, e) : err;
}

/* evaluate x and return its value in environment e */
L eval(L x, L e)
{
    return T(x) == ATOM ? assoc(x, e) : T(x) == CONS ? apply(eval(car(x), e), cdr(x), e) : x;
}

/* tokenization buffer: 256 bytes accommodates atoms up to ~64 emoji (4 bytes each) */
char buf[256];
int see = ' ';

/* advance to the next character */
void look(void)
{
    int c = getchar();
    see = c;
}

/* return nonzero if we are looking at character c, ' ' means any white space */
I seeing(int c)
{
    return c == ' ' ? (see > 0 && see <= c) : see == c;
}

/* return the look ahead character from standard input, advance to the next */
int get(void)
{
    int c = see;
    look();
    return c;
}

/* tokenize into buf[], return first character of buf[] */
char scan(void)
{
    I i = 0;
    while (seeing(' ') || seeing(';'))
    {
        if (seeing(';'))
            while (see >= 0 && !seeing('\n')) /* skip to end of line */
                look();
        else
            look();
    }
    if (see < 0)
    {
        buf[0] = 0;
        return 0;
    } /* EOF */
    if (seeing('(') || seeing(')') || seeing('\'') || seeing('[') || seeing(']'))
        buf[i++] = (char)get();
    else
        do
        {
            /* collect all bytes of the current UTF-8 character before checking
               for token boundaries. utf8_len returns 1 for plain ASCII. */
            int bytes = utf8_len((char)see);
            int b;
            for (b = 0; b < bytes && i < 255; b++)
                buf[i++] = (char)get();
        } while (i < 255 && see >= 0 && !seeing('(') && !seeing(')') && !seeing('[') && !seeing(']') && !seeing(' '));
    buf[i] = 0;
    return *buf;
}

/* return the Lisp expression read from standard input */
L Read(void)
{
    if (!scan())
        exit(0); /* EOF — exit REPL */
    return parse();
}

/* return a parsed Lisp list */
L list(void)
{
    L x;
    if (scan() == ')')
        return nil;
    if (!strcmp(buf, "."))
    {
        x = Read();
        scan();
        return x;
    }
    x = parse();
    return cons(x, list());
}

/* return a parsed Lisp expression x quoted as (quote x) */
L quote(void)
{
    return cons(atom("quote"), cons(Read(), nil));
}

/* return a parsed atomic Lisp expression (a number or an atom) */
L atomic(void)
{
    L n;
    I i;
    return (sscanf(buf, "%lg%n", &n, &i) > 0 && !buf[i]) ? n : atom(buf);
}

/* collect elements of a tensor literal until ']', recursing into parse()
   so that any expression -- atom, number, (s-expr), or nested [tensor] --
   is accepted as an element */
static L tensor_elems(void)
{
    L x;
    if (scan() == ']')
        return nil;
    x = parse();
    return cons(x, tensor_elems());
}

/* parse [ e1 e2 ... ] into a (make-tensor e1 e2 ...) CONS form.
   evaluation happens later, so sub-expressions like (+ 3 x) work fine. */
L tensor_lit(void)
{
    return cons(atom("make-tensor"), tensor_elems());
}

/* return a parsed Lisp expression */
L parse(void)
{
    return *buf == '(' ? list() : *buf == '[' ? tensor_lit() : *buf == '\'' ? quote() : atomic();
}

/* gc_core: discard temporary cells, keeping only the global environment */
void gc_core(void)
{
    sp = ord(env);
}

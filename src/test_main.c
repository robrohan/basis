#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "r2_unit.h"
#include "tinylisp.h"
#include "tinytensor.h"
#include "runtime.h"

int r2_tests_run = 0;

/* Global test state — initialised by setup() before each test */
static lisp_state_t *ts = NULL;

/* Reset the interpreter to a clean state before each test. Calling this at
   the top of every test ensures no state leaks between tests. */
static void setup(void)
{
    II i;
    if (ts) lisp_state_free(ts);
    ts = lisp_state_new();
    ts->l_nil = box(NIL, 0);
    ts->l_err = atom(ts, "L_ERR");
    ts->l_tru = atom(ts, "#t");
    ts->l_env = pair(ts, ts->l_tru, ts->l_tru, ts->l_nil);
    register_tensor_prims(ts);
    register_runtime_prims(ts);
    for (i = 0; ts->prim[i].s; i++)
        ts->l_env = pair(ts, atom(ts, ts->prim[i].s), box(PRIM, i), ts->l_env);
}

/* -----------------------------------------------------------------------
   NaN-boxing: box / ord / equ
   --------------------------------------------------------------------- */

static const char *test_box_tags(void)
{
    /* T() takes the address of its argument so we need lvalue temporaries */
    L a = box(ATOM, 0); r2_assert("box ATOM tag", T(a) == ATOM);
    L p = box(PRIM, 0); r2_assert("box PRIM tag", T(p) == PRIM);
    L c = box(CONS, 0); r2_assert("box CONS tag", T(c) == CONS);
    L k = box(CLOS, 0); r2_assert("box CLOS tag", T(k) == CLOS);
    L n = box(NIL,  0); r2_assert("box NIL  tag", T(n) == NIL);
    return NULL;
}

static const char *test_box_ord_roundtrip(void)
{
    r2_assert("ord 0",   ord(box(ATOM,   0)) ==   0);
    r2_assert("ord 1",   ord(box(ATOM,   1)) ==   1);
    r2_assert("ord 42",  ord(box(CONS,  42)) ==  42);
    r2_assert("ord 999", ord(box(PRIM, 999)) == 999);
    return NULL;
}

static const char *test_equ(void)
{
    r2_assert("same tag+ord equal",      equ(box(ATOM, 5), box(ATOM, 5)));
    r2_assert("diff ord not equal",     !equ(box(ATOM, 5), box(ATOM, 6)));
    r2_assert("diff tag not equal",     !equ(box(ATOM, 5), box(PRIM, 5)));
    r2_assert("same number equal",       equ((L)42.0, (L)42.0));
    r2_assert("diff number not equal",  !equ((L)1.0,  (L)2.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   Atom interning
   --------------------------------------------------------------------- */

static const char *test_atom_interning(void)
{
    setup();
    L a1 = atom(ts, "hello");
    L a2 = atom(ts, "hello");
    L a3 = atom(ts, "world");
    r2_assert("same name interns to same value", equ(a1, a2));
    r2_assert("different names differ",         !equ(a1, a3));
    r2_assert("atom has ATOM tag",               T(a1) == ATOM);
    return NULL;
}

static const char *test_atom_utf8(void)
{
    setup();
    L fire1 = atom(ts, "\xF0\x9F\x94\xA5");   /* 🔥 */
    L fire2 = atom(ts, "\xF0\x9F\x94\xA5");   /* 🔥 same */
    L water = atom(ts, "\xF0\x9F\x92\xA7");   /* 💧 different */
    L pi    = atom(ts, "\xCE\xBB");            /* λ  two-byte */
    r2_assert("emoji interns identically",      equ(fire1, fire2));
    r2_assert("different emoji differ",        !equ(fire1, water));
    r2_assert("emoji has ATOM tag",             T(fire1) == ATOM);
    r2_assert("two-byte rune has ATOM tag",     T(pi) == ATOM);
    r2_assert("two-byte rune interns same",     equ(pi, atom(ts, "\xCE\xBB")));
    return NULL;
}

/* -----------------------------------------------------------------------
   cons / car / cdr
   --------------------------------------------------------------------- */

static const char *test_cons_car_cdr(void)
{
    setup();
    L p = cons(ts, (L)1.0, (L)2.0);
    r2_assert("cons has CONS tag",    T(p) == CONS);
    r2_assert("car returns first",    equ(car(ts, p), (L)1.0));
    r2_assert("cdr returns second",   equ(cdr(ts, p), (L)2.0));
    return NULL;
}

static const char *test_cons_nested(void)
{
    setup();
    /* (1 2 3) as a proper list */
    L lst = cons(ts, (L)1.0, cons(ts, (L)2.0, cons(ts, (L)3.0, ts->l_nil)));
    r2_assert("car of list",          equ(car(ts, lst), (L)1.0));
    r2_assert("cadr of list",         equ(car(ts, cdr(ts, lst)), (L)2.0));
    r2_assert("caddr of list",        equ(car(ts, cdr(ts, cdr(ts, lst))), (L)3.0));
    r2_assert("cdddr of list is nil", is_nil(ts, cdr(ts, cdr(ts, cdr(ts, lst)))));
    return NULL;
}

static const char *test_car_cdr_non_pair(void)
{
    setup();
    r2_assert("car of number is L_ERR", equ(car(ts, (L)42.0), ts->l_err));
    r2_assert("cdr of number is L_ERR", equ(cdr(ts, (L)42.0), ts->l_err));
    r2_assert("car of nil is L_ERR",    equ(car(ts, ts->l_nil), ts->l_err));
    return NULL;
}

/* -----------------------------------------------------------------------
   is_nil
   --------------------------------------------------------------------- */

static const char *test_is_nil(void)
{
    setup();
    r2_assert("nil is nil",                is_nil(ts, ts->l_nil));
    r2_assert("tru is not nil",           !is_nil(ts, ts->l_tru));
    r2_assert("number is not nil",        !is_nil(ts, (L)42.0));
    r2_assert("zero is not nil",          !is_nil(ts, (L)0.0));
    r2_assert("cons cell is not nil",     !is_nil(ts, cons(ts, ts->l_nil, ts->l_nil)));
    r2_assert("L_ERR atom is not nil",      !is_nil(ts, ts->l_err));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — self-evaluating forms
   --------------------------------------------------------------------- */

static const char *test_eval_numbers(void)
{
    setup();
    r2_assert("positive number",  equ(eval(ts, (L)42.0,  ts->l_env), (L)42.0));
    r2_assert("zero",             equ(eval(ts, (L)0.0,   ts->l_env), (L)0.0));
    r2_assert("negative number",  equ(eval(ts, (L)-7.0,  ts->l_env), (L)-7.0));
    r2_assert("float",            equ(eval(ts, (L)3.14,  ts->l_env), (L)3.14));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — arithmetic primitives
   --------------------------------------------------------------------- */

static const char *test_eval_add(void)
{
    setup();
    /* (+ 1 2) => 3 */
    L e1 = cons(ts, atom(ts, "+"), cons(ts, (L)1.0, cons(ts, (L)2.0, ts->l_nil)));
    r2_assert("(+ 1 2) == 3", equ(eval(ts, e1, ts->l_env), (L)3.0));

    /* (+ 1 2 3) => 6 */
    L e2 = cons(ts, atom(ts, "+"), cons(ts, (L)1.0, cons(ts, (L)2.0, cons(ts, (L)3.0, ts->l_nil))));
    r2_assert("(+ 1 2 3) == 6", equ(eval(ts, e2, ts->l_env), (L)6.0));

    /* (+ 0 0) => 0 */
    L e3 = cons(ts, atom(ts, "+"), cons(ts, (L)0.0, cons(ts, (L)0.0, ts->l_nil)));
    r2_assert("(+ 0 0) == 0", equ(eval(ts, e3, ts->l_env), (L)0.0));
    return NULL;
}

static const char *test_eval_sub(void)
{
    setup();
    L e1 = cons(ts, atom(ts, "-"), cons(ts, (L)10.0, cons(ts, (L)3.0, ts->l_nil)));
    r2_assert("(- 10 3) == 7", equ(eval(ts, e1, ts->l_env), (L)7.0));

    /* (- 10 3 2) => 5 */
    L e2 = cons(ts, atom(ts, "-"), cons(ts, (L)10.0, cons(ts, (L)3.0, cons(ts, (L)2.0, ts->l_nil))));
    r2_assert("(- 10 3 2) == 5", equ(eval(ts, e2, ts->l_env), (L)5.0));
    return NULL;
}

static const char *test_eval_mul(void)
{
    setup();
    L e1 = cons(ts, atom(ts, "*"), cons(ts, (L)6.0,  cons(ts, (L)7.0, ts->l_nil)));
    r2_assert("(* 6 7) == 42",  equ(eval(ts, e1, ts->l_env), (L)42.0));

    L e2 = cons(ts, atom(ts, "*"), cons(ts, (L)2.0, cons(ts, (L)3.0, cons(ts, (L)4.0, ts->l_nil))));
    r2_assert("(* 2 3 4) == 24", equ(eval(ts, e2, ts->l_env), (L)24.0));
    return NULL;
}

static const char *test_eval_div(void)
{
    setup();
    L e1 = cons(ts, atom(ts, "/"), cons(ts, (L)10.0, cons(ts, (L)2.0, ts->l_nil)));
    r2_assert("(/ 10 2) == 5", equ(eval(ts, e1, ts->l_env), (L)5.0));

    /* (/ 100 2 5) => 10 */
    L e2 = cons(ts, atom(ts, "/"), cons(ts, (L)100.0, cons(ts, (L)2.0, cons(ts, (L)5.0, ts->l_nil))));
    r2_assert("(/ 100 2 5) == 10", equ(eval(ts, e2, ts->l_env), (L)10.0));
    return NULL;
}

static const char *test_eval_int(void)
{
    setup();
    L e1 = cons(ts, atom(ts, "truncate"), cons(ts, (L)3.9,  ts->l_nil));
    r2_assert("(truncate 3.9) == 3",   equ(eval(ts, e1, ts->l_env), (L)3.0));

    L e2 = cons(ts, atom(ts, "truncate"), cons(ts, (L)-2.7, ts->l_nil));
    r2_assert("(truncate -2.7) == -2", equ(eval(ts, e2, ts->l_env), (L)-2.0));

    L e3 = cons(ts, atom(ts, "truncate"), cons(ts, (L)5.0,  ts->l_nil));
    r2_assert("(truncate 5.0) == 5",   equ(eval(ts, e3, ts->l_env), (L)5.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — comparison and equality
   --------------------------------------------------------------------- */

static const char *test_eval_lt(void)
{
    setup();
    L lt_t = cons(ts, atom(ts, "<"), cons(ts, (L)1.0, cons(ts, (L)2.0, ts->l_nil)));
    L lt_f = cons(ts, atom(ts, "<"), cons(ts, (L)2.0, cons(ts, (L)1.0, ts->l_nil)));
    L lt_e = cons(ts, atom(ts, "<"), cons(ts, (L)2.0, cons(ts, (L)2.0, ts->l_nil)));
    r2_assert("(< 1 2) is tru",   equ(eval(ts, lt_t, ts->l_env), ts->l_tru));
    r2_assert("(< 2 1) is nil",   equ(eval(ts, lt_f, ts->l_env), ts->l_nil));
    r2_assert("(< 2 2) is nil",   equ(eval(ts, lt_e, ts->l_env), ts->l_nil));
    return NULL;
}

static const char *test_eval_eq(void)
{
    setup();
    L eq_t = cons(ts, atom(ts, "equal"), cons(ts, (L)42.0, cons(ts, (L)42.0, ts->l_nil)));
    L eq_f = cons(ts, atom(ts, "equal"), cons(ts, (L)1.0,  cons(ts, (L)2.0,  ts->l_nil)));
    r2_assert("(equal 42 42) is tru", equ(eval(ts, eq_t, ts->l_env), ts->l_tru));
    r2_assert("(equal 1 2) is nil",   equ(eval(ts, eq_f, ts->l_env), ts->l_nil));

    /* atoms compare by identity */
    L sym = atom(ts, "foo");
    L eq_a = cons(ts, atom(ts, "equal"),
                  cons(ts, cons(ts, atom(ts, "quote"), cons(ts, sym, ts->l_nil)),
                       cons(ts, cons(ts, atom(ts, "quote"), cons(ts, sym, ts->l_nil)), ts->l_nil)));
    r2_assert("(equal 'foo 'foo) is tru", equ(eval(ts, eq_a, ts->l_env), ts->l_tru));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — boolean: not / and / or
   --------------------------------------------------------------------- */

static const char *test_eval_not(void)
{
    setup();
    L not_nil = cons(ts, atom(ts, "not"), cons(ts, ts->l_nil, ts->l_nil));
    L not_tru = cons(ts, atom(ts, "not"), cons(ts, ts->l_tru, ts->l_nil));
    L not_num = cons(ts, atom(ts, "not"), cons(ts, (L)42.0, ts->l_nil));
    r2_assert("(not ()) is tru",  equ(eval(ts, not_nil, ts->l_env), ts->l_tru));
    r2_assert("(not #t) is nil",  equ(eval(ts, not_tru, ts->l_env), ts->l_nil));
    r2_assert("(not 42) is nil",  equ(eval(ts, not_num, ts->l_env), ts->l_nil));
    return NULL;
}

static const char *test_eval_and(void)
{
    setup();
    L tt = cons(ts, atom(ts, "and"), cons(ts, ts->l_tru, cons(ts, ts->l_tru,      ts->l_nil)));
    L tf = cons(ts, atom(ts, "and"), cons(ts, ts->l_tru, cons(ts, ts->l_nil,      ts->l_nil)));
    L ff = cons(ts, atom(ts, "and"), cons(ts, ts->l_nil, cons(ts, ts->l_nil,      ts->l_nil)));
    r2_assert("(and #t #t) truthy", !is_nil(ts, eval(ts, tt, ts->l_env)));
    r2_assert("(and #t ()) is nil",  is_nil(ts, eval(ts, tf, ts->l_env)));
    r2_assert("(and () ()) is nil",  is_nil(ts, eval(ts, ff, ts->l_env)));
    return NULL;
}

static const char *test_eval_or(void)
{
    setup();
    L ff = cons(ts, atom(ts, "or"), cons(ts, ts->l_nil, cons(ts, ts->l_nil, ts->l_nil)));
    L tf = cons(ts, atom(ts, "or"), cons(ts, ts->l_tru, cons(ts, ts->l_nil, ts->l_nil)));
    L ft = cons(ts, atom(ts, "or"), cons(ts, ts->l_nil, cons(ts, ts->l_tru, ts->l_nil)));
    r2_assert("(or () ()) is nil",   is_nil(ts, eval(ts, ff, ts->l_env)));
    r2_assert("(or #t ()) is truthy", !is_nil(ts, eval(ts, tf, ts->l_env)));
    r2_assert("(or () #t) is truthy", !is_nil(ts, eval(ts, ft, ts->l_env)));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — quote
   --------------------------------------------------------------------- */

static const char *test_eval_quote(void)
{
    setup();
    L sym  = atom(ts, "hello");
    L expr = cons(ts, atom(ts, "quote"), cons(ts, sym, ts->l_nil));
    r2_assert("(quote hello) returns atom unevaluated", equ(eval(ts, expr, ts->l_env), sym));

    /* quoted list is not evaluated */
    L lst      = cons(ts, (L)1.0, cons(ts, (L)2.0, ts->l_nil));
    L qlst     = cons(ts, atom(ts, "quote"), cons(ts, lst, ts->l_nil));
    L result   = eval(ts, qlst, ts->l_env);
    r2_assert("(quote (1 2)) is a pair",    T(result) == CONS);
    r2_assert("car of quoted list is 1",    equ(car(ts, result), (L)1.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — if / cond
   --------------------------------------------------------------------- */

static const char *test_eval_if(void)
{
    setup();
    /* (if #t 1 2) => 1 */
    L if_t = cons(ts, atom(ts, "if"), cons(ts, ts->l_tru, cons(ts, (L)1.0, cons(ts, (L)2.0, ts->l_nil))));
    r2_assert("(if #t 1 2) == 1", equ(eval(ts, if_t, ts->l_env), (L)1.0));

    /* (if () 1 2) => 2 */
    L if_f = cons(ts, atom(ts, "if"), cons(ts, ts->l_nil, cons(ts, (L)1.0, cons(ts, (L)2.0, ts->l_nil))));
    r2_assert("(if () 1 2) == 2", equ(eval(ts, if_f, ts->l_env), (L)2.0));
    return NULL;
}

static const char *test_eval_cond(void)
{
    setup();
    /* (cond (() 1) (#t 2)) => 2 */
    L c1   = cons(ts, ts->l_nil, cons(ts, (L)1.0, ts->l_nil));
    L c2   = cons(ts, ts->l_tru, cons(ts, (L)2.0, ts->l_nil));
    L expr = cons(ts, atom(ts, "cond"), cons(ts, c1, cons(ts, c2, ts->l_nil)));
    r2_assert("(cond (() 1)(#t 2)) == 2", equ(eval(ts, expr, ts->l_env), (L)2.0));

    /* (cond (#t 99)) => 99 */
    L c3    = cons(ts, ts->l_tru, cons(ts, (L)99.0, ts->l_nil));
    L expr2 = cons(ts, atom(ts, "cond"), cons(ts, c3, ts->l_nil));
    r2_assert("(cond (#t 99)) == 99", equ(eval(ts, expr2, ts->l_env), (L)99.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — pair?
   --------------------------------------------------------------------- */

static const char *test_eval_pair(void)
{
    setup();
    /* (consp '(1 . 2)) => #t */
    L p       = cons(ts, (L)1.0, (L)2.0);
    L qp      = cons(ts, atom(ts, "quote"), cons(ts, p, ts->l_nil));
    L is_pair = cons(ts, atom(ts, "consp"), cons(ts, qp, ts->l_nil));
    r2_assert("(consp '(1 . 2)) is tru", equ(eval(ts, is_pair, ts->l_env), ts->l_tru));

    /* (consp 42) => () */
    L not_pair = cons(ts, atom(ts, "consp"), cons(ts, (L)42.0, ts->l_nil));
    r2_assert("(consp 42) is nil", equ(eval(ts, not_pair, ts->l_env), ts->l_nil));

    /* (consp ()) => () */
    L qnil      = cons(ts, atom(ts, "quote"), cons(ts, ts->l_nil, ts->l_nil));
    L nil_pair  = cons(ts, atom(ts, "consp"), cons(ts, qnil, ts->l_nil));
    r2_assert("(consp '()) is nil", equ(eval(ts, nil_pair, ts->l_env), ts->l_nil));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — cons / car / cdr as Lisp primitives
   --------------------------------------------------------------------- */

static const char *test_eval_cons_car_cdr(void)
{
    setup();
    /* (car (cons 10 20)) => 10 */
    L inner    = cons(ts, atom(ts, "cons"), cons(ts, (L)10.0, cons(ts, (L)20.0, ts->l_nil)));
    L car_expr = cons(ts, atom(ts, "car"),  cons(ts, inner,   ts->l_nil));
    r2_assert("(car (cons 10 20)) == 10", equ(eval(ts, car_expr, ts->l_env), (L)10.0));

    /* (cdr (cons 10 20)) => 20 */
    L inner2   = cons(ts, atom(ts, "cons"), cons(ts, (L)10.0, cons(ts, (L)20.0, ts->l_nil)));
    L cdr_expr = cons(ts, atom(ts, "cdr"),  cons(ts, inner2,  ts->l_nil));
    r2_assert("(cdr (cons 10 20)) == 20", equ(eval(ts, cdr_expr, ts->l_env), (L)20.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — lambda and closures
   --------------------------------------------------------------------- */

static const char *test_eval_lambda(void)
{
    setup();
    /* ((lambda (x) (* x x)) 5) => 25 */
    L body  = cons(ts, atom(ts, "*"), cons(ts, atom(ts, "x"), cons(ts, atom(ts, "x"), ts->l_nil)));
    L lam   = cons(ts, atom(ts, "lambda"), cons(ts, cons(ts, atom(ts, "x"), ts->l_nil), cons(ts, body, ts->l_nil)));
    L call  = cons(ts, lam, cons(ts, (L)5.0, ts->l_nil));
    r2_assert("((lambda (x) (* x x)) 5) == 25", equ(eval(ts, call, ts->l_env), (L)25.0));
    return NULL;
}

static const char *test_eval_lambda_multi_arg(void)
{
    setup();
    /* ((lambda (x y) (+ x y)) 3 4) => 7 */
    L body  = cons(ts, atom(ts, "+"), cons(ts, atom(ts, "x"), cons(ts, atom(ts, "y"), ts->l_nil)));
    L args  = cons(ts, atom(ts, "x"), cons(ts, atom(ts, "y"), ts->l_nil));
    L lam   = cons(ts, atom(ts, "lambda"), cons(ts, args, cons(ts, body, ts->l_nil)));
    L call  = cons(ts, lam, cons(ts, (L)3.0, cons(ts, (L)4.0, ts->l_nil)));
    r2_assert("((lambda (x y) (+ x y)) 3 4) == 7", equ(eval(ts, call, ts->l_env), (L)7.0));
    return NULL;
}

static const char *test_eval_closure_captures(void)
{
    setup();
    /* ((lambda (x) (lambda (y) (+ x y))) 10) => closure, then apply to 5 => 15 */
    L inner_body = cons(ts, atom(ts, "+"), cons(ts, atom(ts, "x"), cons(ts, atom(ts, "y"), ts->l_nil)));
    L inner_lam  = cons(ts, atom(ts, "lambda"), cons(ts, cons(ts, atom(ts, "y"), ts->l_nil), cons(ts, inner_body, ts->l_nil)));
    L outer_lam  = cons(ts, atom(ts, "lambda"), cons(ts, cons(ts, atom(ts, "x"), ts->l_nil), cons(ts, inner_lam, ts->l_nil)));
    L outer_call = cons(ts, outer_lam, cons(ts, (L)10.0, ts->l_nil));
    L adder      = eval(ts, outer_call, ts->l_env);
    r2_assert("outer returns a closure", T(adder) == CLOS);
    L inner_call = cons(ts, adder, cons(ts, (L)5.0, ts->l_nil));  /* can't use eval directly on cons(ts, adder,...) */
    /* apply the closure */
    L result = apply(ts, adder, cons(ts, (L)5.0, ts->l_nil), ts->l_env);
    r2_assert("closure captures x=10, (adder 5) == 15", equ(result, (L)15.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — define
   --------------------------------------------------------------------- */

static const char *test_eval_define(void)
{
    setup();
    /* (define answer 42) then answer => 42 */
    L def = cons(ts, atom(ts, "define"), cons(ts, atom(ts, "answer"), cons(ts, (L)42.0, ts->l_nil)));
    eval(ts, def, ts->l_env);
    r2_assert("defined value resolves", equ(eval(ts, atom(ts, "answer"), ts->l_env), (L)42.0));
    return NULL;
}

static const char *test_eval_define_lambda(void)
{
    setup();
    /* (define sq (lambda (x) (* x x))) then (sq 9) => 81 */
    L body   = cons(ts, atom(ts, "*"), cons(ts, atom(ts, "x"), cons(ts, atom(ts, "x"), ts->l_nil)));
    L lam    = cons(ts, atom(ts, "lambda"), cons(ts, cons(ts, atom(ts, "x"), ts->l_nil), cons(ts, body, ts->l_nil)));
    L def    = cons(ts, atom(ts, "define"), cons(ts, atom(ts, "sq"), cons(ts, lam, ts->l_nil)));
    eval(ts, def, ts->l_env);
    L call   = cons(ts, atom(ts, "sq"), cons(ts, (L)9.0, ts->l_nil));
    r2_assert("(sq 9) == 81 after define", equ(eval(ts, call, ts->l_env), (L)81.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — let*
   --------------------------------------------------------------------- */

static const char *test_eval_leta(void)
{
    setup();
    /* (let* (x 3) (y 4) (+ x y)) => 7 */
    L body  = cons(ts, atom(ts, "+"), cons(ts, atom(ts, "x"), cons(ts, atom(ts, "y"), ts->l_nil)));
    L b2    = cons(ts, cons(ts, atom(ts, "y"), cons(ts, (L)4.0, ts->l_nil)), cons(ts, body, ts->l_nil));
    L b1    = cons(ts, cons(ts, atom(ts, "x"), cons(ts, (L)3.0, ts->l_nil)), b2);
    L expr  = cons(ts, atom(ts, "let*"), b1);
    r2_assert("(let* (x 3)(y 4)(+ x y)) == 7", equ(eval(ts, expr, ts->l_env), (L)7.0));
    return NULL;
}

static const char *test_eval_leta_sequential(void)
{
    setup();
    /* (let* (x 2) (y (* x 3)) y) => 6   y depends on x */
    L y_body = cons(ts, atom(ts, "*"), cons(ts, atom(ts, "x"), cons(ts, (L)3.0, ts->l_nil)));
    L b2     = cons(ts, cons(ts, atom(ts, "y"), cons(ts, y_body, ts->l_nil)), cons(ts, atom(ts, "y"), ts->l_nil));
    L b1     = cons(ts, cons(ts, atom(ts, "x"), cons(ts, (L)2.0, ts->l_nil)), b2);
    L expr   = cons(ts, atom(ts, "let*"), b1);
    r2_assert("(let* (x 2)(y (* x 3)) y) == 6", equ(eval(ts, expr, ts->l_env), (L)6.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — recursion via define
   --------------------------------------------------------------------- */

static const char *test_eval_recursion(void)
{
    setup();
    /* (define fact (lambda (n) (if (< n 2) 1 (* n (fact (- n 1))))))
       (fact 5) => 120 */
    L base   = (L)1.0;
    L rec_call = cons(ts, atom(ts, "fact"),
                      cons(ts, cons(ts, atom(ts, "-"), cons(ts, atom(ts, "n"), cons(ts, (L)1.0, ts->l_nil))), ts->l_nil));
    L mul_expr = cons(ts, atom(ts, "*"), cons(ts, atom(ts, "n"), cons(ts, rec_call, ts->l_nil)));
    L body   = cons(ts, atom(ts, "if"),
                    cons(ts, cons(ts, atom(ts, "<"), cons(ts, atom(ts, "n"), cons(ts, (L)2.0, ts->l_nil))),
                         cons(ts, base, cons(ts, mul_expr, ts->l_nil))));
    L lam    = cons(ts, atom(ts, "lambda"), cons(ts, cons(ts, atom(ts, "n"), ts->l_nil), cons(ts, body, ts->l_nil)));
    L def    = cons(ts, atom(ts, "define"), cons(ts, atom(ts, "fact"), cons(ts, lam, ts->l_nil)));
    eval(ts, def, ts->l_env);
    L call   = cons(ts, atom(ts, "fact"), cons(ts, (L)5.0, ts->l_nil));
    r2_assert("(fact 5) == 120", equ(eval(ts, call, ts->l_env), (L)120.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — UTF-8 atom names
   --------------------------------------------------------------------- */

static const char *test_eval_utf8_atoms(void)
{
    setup();

    /* (define 🔥 42) then 🔥 => 42 */
    L def1 = cons(ts, atom(ts, "define"), cons(ts, atom(ts, "\xF0\x9F\x94\xA5"), cons(ts, (L)42.0, ts->l_nil)));
    eval(ts, def1, ts->l_env);
    r2_assert("emoji atom resolves",
              equ(eval(ts, atom(ts, "\xF0\x9F\x94\xA5"), ts->l_env), (L)42.0));

    /* (define λ (lambda (x) (* x 2))) then (λ 21) => 42 */
    L body = cons(ts, atom(ts, "*"), cons(ts, atom(ts, "x"), cons(ts, (L)2.0, ts->l_nil)));
    L lam  = cons(ts, atom(ts, "lambda"), cons(ts, cons(ts, atom(ts, "x"), ts->l_nil), cons(ts, body, ts->l_nil)));
    L def2 = cons(ts, atom(ts, "define"), cons(ts, atom(ts, "\xCE\xBB"), cons(ts, lam, ts->l_nil)));
    eval(ts, def2, ts->l_env);
    L call = cons(ts, atom(ts, "\xCE\xBB"), cons(ts, (L)21.0, ts->l_nil));
    r2_assert("(λ 21) == 42", equ(eval(ts, call, ts->l_env), (L)42.0));

    /* Greek π as a value */
    L def3 = cons(ts, atom(ts, "define"), cons(ts, atom(ts, "\xCF\x80"), cons(ts, (L)3.14159, ts->l_nil)));
    eval(ts, def3, ts->l_env);
    r2_assert("π resolves to 3.14159",
              equ(eval(ts, atom(ts, "\xCF\x80"), ts->l_env), (L)3.14159));
    return NULL;
}

/* -----------------------------------------------------------------------
   tensor — helpers
   --------------------------------------------------------------------- */

/* Build a TENS L value from a flat float array inline */
static L make_vec(II len, const float *data)
{
    II shape[1];
    shape[0] = len;
    return box(TENS, (II)(alloc_tensor(ts, 1, shape, len, data) - ts->tensor_heap));
}

static L make_mat(II rows, II cols, const float *data)
{
    II shape[2];
    shape[0] = rows;
    shape[1] = cols;
    return box(TENS, (II)(alloc_tensor(ts, 2, shape, rows * cols, data) - ts->tensor_heap));
}

/* -----------------------------------------------------------------------
   tensor — NaN-box tag
   --------------------------------------------------------------------- */

static const char *test_tens_tag(void)
{
    setup();
    float d[] = {1.f, 2.f, 3.f};
    L v = make_vec(3, d);
    r2_assert("tensor has TENS tag", T(v) == TENS);
    return NULL;
}

/* -----------------------------------------------------------------------
   tensor — tensor? predicate
   --------------------------------------------------------------------- */

static const char *test_tensor_predicate(void)
{
    setup();
    float d[] = {1.f, 2.f};
    L v = make_vec(2, d);
    /* (tensorp v) => #t */
    L expr_t = cons(ts, atom(ts, "tensorp"), cons(ts, v, ts->l_nil));
    r2_assert("(tensorp [1 2]) is tru",   equ(eval(ts, expr_t, ts->l_env), ts->l_tru));
    /* (tensorp 42) => () */
    L expr_f = cons(ts, atom(ts, "tensorp"), cons(ts, (L)42.0, ts->l_nil));
    r2_assert("(tensorp 42) is nil",      equ(eval(ts, expr_f, ts->l_env), ts->l_nil));
    return NULL;
}

/* -----------------------------------------------------------------------
   tensor — shape and rank
   --------------------------------------------------------------------- */

static const char *test_tensor_shape_rank_vec(void)
{
    setup();
    float d[] = {10.f, 20.f, 30.f};
    L v = make_vec(3, d);

    /* (rank v) => 1 */
    L rank_expr = cons(ts, atom(ts, "rank"), cons(ts, v, ts->l_nil));
    r2_assert("rank of vec is 1", equ(eval(ts, rank_expr, ts->l_env), (L)1.0));

    /* (shape v) => [3] */
    L shape_expr = cons(ts, atom(ts, "shape"), cons(ts, v, ts->l_nil));
    L sh = eval(ts, shape_expr, ts->l_env);
    r2_assert("shape of vec is TENS",        T(sh) == TENS);
    r2_assert("shape[0] of [10 20 30] == 3", ts->tensor_heap[ord(sh)].data[0] == 3.f);
    return NULL;
}

static const char *test_tensor_shape_rank_mat(void)
{
    setup();
    float d[] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
    L m = make_mat(2, 3, d);

    /* (rank m) => 2 */
    L rank_expr = cons(ts, atom(ts, "rank"), cons(ts, m, ts->l_nil));
    r2_assert("rank of 2x3 mat is 2", equ(eval(ts, rank_expr, ts->l_env), (L)2.0));

    /* (shape m) => [2 3] */
    L shape_expr = cons(ts, atom(ts, "shape"), cons(ts, m, ts->l_nil));
    L sh = eval(ts, shape_expr, ts->l_env);
    r2_assert("shape[0] of 2x3 == 2", ts->tensor_heap[ord(sh)].data[0] == 2.f);
    r2_assert("shape[1] of 2x3 == 3", ts->tensor_heap[ord(sh)].data[1] == 3.f);
    return NULL;
}

/* -----------------------------------------------------------------------
   tensor — slice
   --------------------------------------------------------------------- */

static const char *test_tensor_slice_vec(void)
{
    setup();
    float d[] = {10.f, 20.f, 30.f};
    L v = make_vec(3, d);

    L s0 = cons(ts, atom(ts, "slice"), cons(ts, v, cons(ts, (L)0.0, ts->l_nil)));
    L s1 = cons(ts, atom(ts, "slice"), cons(ts, v, cons(ts, (L)1.0, ts->l_nil)));
    L s2 = cons(ts, atom(ts, "slice"), cons(ts, v, cons(ts, (L)2.0, ts->l_nil)));
    r2_assert("slice 0 == 10", equ(eval(ts, s0, ts->l_env), (L)10.0));
    r2_assert("slice 1 == 20", equ(eval(ts, s1, ts->l_env), (L)20.0));
    r2_assert("slice 2 == 30", equ(eval(ts, s2, ts->l_env), (L)30.0));
    return NULL;
}

static const char *test_tensor_slice_mat(void)
{
    setup();
    /* [[1 2 3][4 5 6]] — slice 0 => [1 2 3], slice 1 => [4 5 6] */
    float d[] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
    L m = make_mat(2, 3, d);

    L row0_expr = cons(ts, atom(ts, "slice"), cons(ts, m, cons(ts, (L)0.0, ts->l_nil)));
    L row0 = eval(ts, row0_expr, ts->l_env);
    r2_assert("row0 is TENS",      T(row0) == TENS);
    r2_assert("row0 len == 3",     ts->tensor_heap[ord(row0)].len == 3);
    r2_assert("row0[0] == 1",      ts->tensor_heap[ord(row0)].data[0] == 1.f);
    r2_assert("row0[2] == 3",      ts->tensor_heap[ord(row0)].data[2] == 3.f);

    L row1_expr = cons(ts, atom(ts, "slice"), cons(ts, m, cons(ts, (L)1.0, ts->l_nil)));
    L row1 = eval(ts, row1_expr, ts->l_env);
    r2_assert("row1[0] == 4",      ts->tensor_heap[ord(row1)].data[0] == 4.f);
    return NULL;
}

/* -----------------------------------------------------------------------
   tensor — element-wise arithmetic
   --------------------------------------------------------------------- */

static const char *test_tensor_add(void)
{
    setup();
    float a[] = {1.f, 2.f, 3.f};
    float b[] = {4.f, 5.f, 6.f};
    L va = make_vec(3, a);
    L vb = make_vec(3, b);

    L expr = cons(ts, atom(ts, "+"), cons(ts, va, cons(ts, vb, ts->l_nil)));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("vec add is TENS",    T(r) == TENS);
    r2_assert("1+4 == 5",           ts->tensor_heap[ord(r)].data[0] == 5.f);
    r2_assert("2+5 == 7",           ts->tensor_heap[ord(r)].data[1] == 7.f);
    r2_assert("3+6 == 9",           ts->tensor_heap[ord(r)].data[2] == 9.f);
    return NULL;
}

static const char *test_tensor_sub(void)
{
    setup();
    float a[] = {10.f, 20.f, 30.f};
    float b[] = {1.f,  2.f,  3.f};
    L va = make_vec(3, a);
    L vb = make_vec(3, b);

    L expr = cons(ts, atom(ts, "-"), cons(ts, va, cons(ts, vb, ts->l_nil)));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("10-1 == 9",  ts->tensor_heap[ord(r)].data[0] == 9.f);
    r2_assert("20-2 == 18", ts->tensor_heap[ord(r)].data[1] == 18.f);
    r2_assert("30-3 == 27", ts->tensor_heap[ord(r)].data[2] == 27.f);
    return NULL;
}

static const char *test_tensor_mul(void)
{
    setup();
    float a[] = {2.f, 3.f, 4.f};
    float b[] = {1.f, 2.f, 3.f};
    L va = make_vec(3, a);
    L vb = make_vec(3, b);

    L expr = cons(ts, atom(ts, "*"), cons(ts, va, cons(ts, vb, ts->l_nil)));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("2*1 == 2",   ts->tensor_heap[ord(r)].data[0] == 2.f);
    r2_assert("3*2 == 6",   ts->tensor_heap[ord(r)].data[1] == 6.f);
    r2_assert("4*3 == 12",  ts->tensor_heap[ord(r)].data[2] == 12.f);
    return NULL;
}

static const char *test_tensor_div(void)
{
    setup();
    float a[] = {10.f, 20.f, 30.f};
    float b[] = {2.f,  4.f,  5.f};
    L va = make_vec(3, a);
    L vb = make_vec(3, b);

    L expr = cons(ts, atom(ts, "/"), cons(ts, va, cons(ts, vb, ts->l_nil)));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("10/2 == 5",  ts->tensor_heap[ord(r)].data[0] == 5.f);
    r2_assert("20/4 == 5",  ts->tensor_heap[ord(r)].data[1] == 5.f);
    r2_assert("30/5 == 6",  ts->tensor_heap[ord(r)].data[2] == 6.f);
    return NULL;
}

/* -----------------------------------------------------------------------
   tensor — scalar broadcast
   --------------------------------------------------------------------- */

static const char *test_tensor_scalar_broadcast(void)
{
    setup();
    float d[] = {1.f, 2.f, 3.f};
    L v = make_vec(3, d);

    L add_expr = cons(ts, atom(ts, "+"), cons(ts, v, cons(ts, (L)10.0, ts->l_nil)));
    L ra = eval(ts, add_expr, ts->l_env);
    r2_assert("broadcast + [0] == 11", ts->tensor_heap[ord(ra)].data[0] == 11.f);
    r2_assert("broadcast + [2] == 13", ts->tensor_heap[ord(ra)].data[2] == 13.f);

    L mul_expr = cons(ts, atom(ts, "*"), cons(ts, v, cons(ts, (L)2.0, ts->l_nil)));
    L rm = eval(ts, mul_expr, ts->l_env);
    r2_assert("broadcast * [0] == 2",  ts->tensor_heap[ord(rm)].data[0] == 2.f);
    r2_assert("broadcast * [1] == 4",  ts->tensor_heap[ord(rm)].data[1] == 4.f);
    r2_assert("broadcast * [2] == 6",  ts->tensor_heap[ord(rm)].data[2] == 6.f);
    return NULL;
}

/* -----------------------------------------------------------------------
   tensor — matrix element-wise arithmetic
   --------------------------------------------------------------------- */

static const char *test_tensor_mat_add(void)
{
    setup();
    float a[] = {1.f, 2.f, 3.f, 4.f};
    float b[] = {10.f, 20.f, 30.f, 40.f};
    L ma = make_mat(2, 2, a);
    L mb = make_mat(2, 2, b);

    L expr = cons(ts, atom(ts, "+"), cons(ts, ma, cons(ts, mb, ts->l_nil)));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("mat add is TENS",     T(r) == TENS);
    r2_assert("mat add rank == 2",   ts->tensor_heap[ord(r)].rank == 2);
    r2_assert("mat [0,0] == 11",     ts->tensor_heap[ord(r)].data[0] == 11.f);
    r2_assert("mat [1,1] == 44",     ts->tensor_heap[ord(r)].data[3] == 44.f);
    return NULL;
}

/* -----------------------------------------------------------------------
   tensor — define and use in expressions
   --------------------------------------------------------------------- */

static const char *test_tensor_define(void)
{
    setup();
    float d[] = {3.f, 1.f, 4.f, 1.f, 5.f};
    L v = make_vec(5, d);

    /* (define pi-vec [3 1 4 1 5]) */
    L def = cons(ts, atom(ts, "define"), cons(ts, atom(ts, "pi-vec"), cons(ts, v, ts->l_nil)));
    eval(ts, def, ts->l_env);

    /* (shape pi-vec) => [5] */
    L shape_expr = cons(ts, atom(ts, "shape"), cons(ts, atom(ts, "pi-vec"), ts->l_nil));
    L sh = eval(ts, shape_expr, ts->l_env);
    r2_assert("shape of pi-vec == [5]", ts->tensor_heap[ord(sh)].data[0] == 5.f);

    /* (slice pi-vec 2) => 4 */
    L sl = cons(ts, atom(ts, "slice"), cons(ts, atom(ts, "pi-vec"), cons(ts, (L)2.0, ts->l_nil)));
    r2_assert("slice 2 of pi-vec == 4", equ(eval(ts, sl, ts->l_env), (L)4.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   tensor — matmul
   --------------------------------------------------------------------- */

static const char *test_tensor_matmul_square(void)
{
    setup();
    /* [[1 2][3 4]] * [[5 6][7 8]] = [[19 22][43 50]] */
    float a[] = {1.f, 2.f, 3.f, 4.f};
    float b[] = {5.f, 6.f, 7.f, 8.f};
    L ma = make_mat(2, 2, a);
    L mb = make_mat(2, 2, b);

    L expr = cons(ts, atom(ts, "matmul"), cons(ts, ma, cons(ts, mb, ts->l_nil)));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("matmul result is TENS",   T(r) == TENS);
    r2_assert("matmul rank == 2",        ts->tensor_heap[ord(r)].rank == 2);
    r2_assert("[0,0] == 19",             ts->tensor_heap[ord(r)].data[0] == 19.f);
    r2_assert("[0,1] == 22",             ts->tensor_heap[ord(r)].data[1] == 22.f);
    r2_assert("[1,0] == 43",             ts->tensor_heap[ord(r)].data[2] == 43.f);
    r2_assert("[1,1] == 50",             ts->tensor_heap[ord(r)].data[3] == 50.f);
    return NULL;
}

static const char *test_tensor_matmul_rect(void)
{
    setup();
    /* [[1 2 3][4 5 6]] (2x3) * [[7 8][9 10][11 12]] (3x2) = [[58 64][139 154]] */
    float a[] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
    float b[] = {7.f, 8.f, 9.f, 10.f, 11.f, 12.f};
    L ma = make_mat(2, 3, a);
    L mb = make_mat(3, 2, b);

    L expr = cons(ts, atom(ts, "matmul"), cons(ts, ma, cons(ts, mb, ts->l_nil)));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("rect matmul shape[0] == 2", ts->tensor_heap[ord(r)].shape[0] == 2);
    r2_assert("rect matmul shape[1] == 2", ts->tensor_heap[ord(r)].shape[1] == 2);
    r2_assert("[0,0] == 58",               ts->tensor_heap[ord(r)].data[0] == 58.f);
    r2_assert("[0,1] == 64",               ts->tensor_heap[ord(r)].data[1] == 64.f);
    r2_assert("[1,0] == 139",              ts->tensor_heap[ord(r)].data[2] == 139.f);
    r2_assert("[1,1] == 154",              ts->tensor_heap[ord(r)].data[3] == 154.f);
    return NULL;
}

static const char *test_tensor_matmul_matvec(void)
{
    setup();
    /* [[1 2 3][4 5 6]] (2x3) * [1 0 0] => [1 4] */
    float a[] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
    float v[] = {1.f, 0.f, 0.f};
    L ma = make_mat(2, 3, a);
    L mv = make_vec(3, v);

    L expr = cons(ts, atom(ts, "matmul"), cons(ts, ma, cons(ts, mv, ts->l_nil)));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("mat-vec result is TENS",  T(r) == TENS);
    r2_assert("mat-vec rank == 1",       ts->tensor_heap[ord(r)].rank == 1);
    r2_assert("result[0] == 1",          ts->tensor_heap[ord(r)].data[0] == 1.f);
    r2_assert("result[1] == 4",          ts->tensor_heap[ord(r)].data[1] == 4.f);
    return NULL;
}

static const char *test_tensor_matmul_vecmat(void)
{
    setup();
    /* [1 2 3] (1x3) * [[1 0][0 1][2 3]] (3x2) => [7 11] */
    float v[] = {1.f, 2.f, 3.f};
    float b[] = {1.f, 0.f, 0.f, 1.f, 2.f, 3.f};
    L mv = make_vec(3, v);
    L mb = make_mat(3, 2, b);

    L expr = cons(ts, atom(ts, "@"), cons(ts, mv, cons(ts, mb, ts->l_nil)));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("vec-mat result is TENS",  T(r) == TENS);
    r2_assert("vec-mat rank == 1",       ts->tensor_heap[ord(r)].rank == 1);
    r2_assert("result[0] == 7",          ts->tensor_heap[ord(r)].data[0] == 7.f);
    r2_assert("result[1] == 11",         ts->tensor_heap[ord(r)].data[1] == 11.f);
    return NULL;
}

static const char *test_tensor_transpose(void)
{
    setup();
    /* transpose [[1 2 3][4 5 6]] (2x3) => [[1 4][2 5][3 6]] (3x2) */
    float a[] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
    L m = make_mat(2, 3, a);

    L expr = cons(ts, atom(ts, "transpose"), cons(ts, m, ts->l_nil));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("transpose is TENS",       T(r) == TENS);
    r2_assert("transpose shape[0] == 3", ts->tensor_heap[ord(r)].shape[0] == 3);
    r2_assert("transpose shape[1] == 2", ts->tensor_heap[ord(r)].shape[1] == 2);
    r2_assert("[0,0] == 1",              ts->tensor_heap[ord(r)].data[0] == 1.f);
    r2_assert("[0,1] == 4",              ts->tensor_heap[ord(r)].data[1] == 4.f);
    r2_assert("[1,0] == 2",              ts->tensor_heap[ord(r)].data[2] == 2.f);
    r2_assert("[2,1] == 6",              ts->tensor_heap[ord(r)].data[5] == 6.f);
    return NULL;
}

/* -----------------------------------------------------------------------
   tensor — vecn primitives
   --------------------------------------------------------------------- */

static const char *test_tensor_abs(void)
{
    setup();
    float d[] = {-3.f, 1.f, -2.f};
    L v = make_vec(3, d);
    L r = eval(ts, cons(ts, atom(ts, "abs"), cons(ts, v, ts->l_nil)), ts->l_env);
    r2_assert("abs[0] == 3", ts->tensor_heap[ord(r)].data[0] == 3.f);
    r2_assert("abs[1] == 1", ts->tensor_heap[ord(r)].data[1] == 1.f);
    r2_assert("abs[2] == 2", ts->tensor_heap[ord(r)].data[2] == 2.f);
    return NULL;
}

static const char *test_tensor_sqrt(void)
{
    setup();
    float d[] = {4.f, 9.f, 16.f};
    L v = make_vec(3, d);
    L r = eval(ts, cons(ts, atom(ts, "sqrt"), cons(ts, v, ts->l_nil)), ts->l_env);
    r2_assert("sqrt[0] == 2", ts->tensor_heap[ord(r)].data[0] == 2.f);
    r2_assert("sqrt[1] == 3", ts->tensor_heap[ord(r)].data[1] == 3.f);
    r2_assert("sqrt[2] == 4", ts->tensor_heap[ord(r)].data[2] == 4.f);
    return NULL;
}

static const char *test_tensor_normalize(void)
{
    setup();
    float d[] = {3.f, 4.f};
    L v = make_vec(2, d);
    L r = eval(ts, cons(ts, atom(ts, "normalize"), cons(ts, v, ts->l_nil)), ts->l_env);
    r2_assert("normalize[0] == 0.6", ts->tensor_heap[ord(r)].data[0] == 0.6f);
    r2_assert("normalize[1] == 0.8", ts->tensor_heap[ord(r)].data[1] == 0.8f);
    return NULL;
}

static const char *test_tensor_pow(void)
{
    setup();
    float d[] = {2.f, 3.f, 4.f};
    L v = make_vec(3, d);
    L r = eval(ts, cons(ts, atom(ts, "pow"), cons(ts, v, cons(ts, (L)2.0, ts->l_nil))), ts->l_env);
    r2_assert("pow[0] == 4",  ts->tensor_heap[ord(r)].data[0] == 4.f);
    r2_assert("pow[1] == 9",  ts->tensor_heap[ord(r)].data[1] == 9.f);
    r2_assert("pow[2] == 16", ts->tensor_heap[ord(r)].data[2] == 16.f);
    return NULL;
}

static const char *test_tensor_zero(void)
{
    setup();
    L r = eval(ts, cons(ts, atom(ts, "zero"), cons(ts, (L)4.0, ts->l_nil)), ts->l_env);
    r2_assert("zero is TENS",     T(r) == TENS);
    r2_assert("zero len == 4",    ts->tensor_heap[ord(r)].len == 4);
    r2_assert("zero[0] == 0",     ts->tensor_heap[ord(r)].data[0] == 0.f);
    r2_assert("zero[3] == 0",     ts->tensor_heap[ord(r)].data[3] == 0.f);
    return NULL;
}

static const char *test_tensor_dot(void)
{
    setup();
    float a[] = {1.f, 2.f, 3.f};
    float b[] = {4.f, 5.f, 6.f};
    L va = make_vec(3, a), vb = make_vec(3, b);
    L r = eval(ts, cons(ts, atom(ts, "dot"), cons(ts, va, cons(ts, vb, ts->l_nil))), ts->l_env);
    r2_assert("dot [1 2 3].[4 5 6] == 32", equ(r, (L)32.0));
    return NULL;
}

static const char *test_tensor_length(void)
{
    setup();
    float d[] = {3.f, 4.f};
    L v = make_vec(2, d);
    L len  = eval(ts, cons(ts, atom(ts, "norm"),  cons(ts, v, ts->l_nil)), ts->l_env);
    L len2 = eval(ts, cons(ts, atom(ts, "norm2"), cons(ts, v, ts->l_nil)), ts->l_env);
    r2_assert("norm [3 4] == 5",   equ(len,  (L)5.0));
    r2_assert("norm2 [3 4] == 25", equ(len2, (L)25.0));
    return NULL;
}

static const char *test_tensor_dist(void)
{
    setup();
    float a[] = {0.f, 0.f};
    float b[] = {3.f, 4.f};
    L va = make_vec(2, a), vb = make_vec(2, b);
    L d  = eval(ts, cons(ts, atom(ts, "dist"),  cons(ts, va, cons(ts, vb, ts->l_nil))), ts->l_env);
    L d2 = eval(ts, cons(ts, atom(ts, "dist2"), cons(ts, va, cons(ts, vb, ts->l_nil))), ts->l_env);
    r2_assert("dist [0 0] [3 4] == 5",   equ(d,  (L)5.0));
    r2_assert("dist2 [0 0] [3 4] == 25", equ(d2, (L)25.0));
    return NULL;
}

static const char *test_tensor_veq(void)
{
    setup();
    float a[] = {1.f, 2.f, 3.f};
    float b[] = {1.f, 2.f, 3.f};
    float c[] = {1.f, 2.f, 4.f};
    L va = make_vec(3, a), vb = make_vec(3, b), vc = make_vec(3, c);
    L eq  = eval(ts, cons(ts, atom(ts, "equalp"), cons(ts, va, cons(ts, vb, ts->l_nil))), ts->l_env);
    L neq = eval(ts, cons(ts, atom(ts, "equalp"), cons(ts, va, cons(ts, vc, ts->l_nil))), ts->l_env);
    r2_assert("equalp equal tensors is #t",     equ(eq, ts->l_tru));
    r2_assert("equalp unequal tensors is nil",  equ(neq, ts->l_nil));
    return NULL;
}

/* -----------------------------------------------------------------------
   tensor — head / tail
   --------------------------------------------------------------------- */

static const char *test_tensor_head_tail_vec(void)
{
    setup();
    float d[] = {10.f, 20.f, 30.f};
    L v = make_vec(3, d);

    L h = eval(ts, cons(ts, atom(ts, "first"), cons(ts, v, ts->l_nil)), ts->l_env);
    r2_assert("first of vec == 10", equ(h, (L)10.0));

    L tl = eval(ts, cons(ts, atom(ts, "rest"), cons(ts, v, ts->l_nil)), ts->l_env);
    r2_assert("rest is TENS",        T(tl) == TENS);
    r2_assert("rest len == 2",       ts->tensor_heap[ord(tl)].len == 2);
    r2_assert("rest[0] == 20",       ts->tensor_heap[ord(tl)].data[0] == 20.f);
    r2_assert("rest[1] == 30",       ts->tensor_heap[ord(tl)].data[1] == 30.f);
    return NULL;
}

static const char *test_tensor_head_tail_mat(void)
{
    setup();
    float d[] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
    L m = make_mat(2, 3, d);

    /* first of 2x3 => first row [1 2 3] */
    L h = eval(ts, cons(ts, atom(ts, "first"), cons(ts, m, ts->l_nil)), ts->l_env);
    r2_assert("first of mat is TENS",     T(h) == TENS);
    r2_assert("first row rank == 1",      ts->tensor_heap[ord(h)].rank == 1);
    r2_assert("first row[0] == 1",        ts->tensor_heap[ord(h)].data[0] == 1.f);
    r2_assert("first row[2] == 3",        ts->tensor_heap[ord(h)].data[2] == 3.f);

    /* rest of 2x3 => [[4 5 6]] (1x3) */
    L tl = eval(ts, cons(ts, atom(ts, "rest"), cons(ts, m, ts->l_nil)), ts->l_env);
    r2_assert("rest of mat is TENS",     T(tl) == TENS);
    r2_assert("rest shape[0] == 1",      ts->tensor_heap[ord(tl)].shape[0] == 1);
    r2_assert("rest shape[1] == 3",      ts->tensor_heap[ord(tl)].shape[1] == 3);
    r2_assert("rest[0][0] == 4",         ts->tensor_heap[ord(tl)].data[0] == 4.f);
    return NULL;
}

/* -----------------------------------------------------------------------
   tensor — vec2/vec4 fast paths
   --------------------------------------------------------------------- */

static const char *test_tensor_fastpath_vec2(void)
{
    setup();
    float a[] = {3.f, 4.f};
    float b[] = {0.f, 0.f};
    L va = make_vec(2, a), vb = make_vec(2, b);

    r2_assert("dot vec2",    equ(eval(ts, cons(ts, atom(ts, "dot"),   cons(ts, va, cons(ts, va, ts->l_nil))), ts->l_env), (L)25.0));
    r2_assert("norm vec2",   equ(eval(ts, cons(ts, atom(ts, "norm"),  cons(ts, va, ts->l_nil)),           ts->l_env), (L)5.0));
    r2_assert("norm2 vec2",  equ(eval(ts, cons(ts, atom(ts, "norm2"), cons(ts, va, ts->l_nil)),           ts->l_env), (L)25.0));
    r2_assert("dist vec2",    equ(eval(ts, cons(ts, atom(ts, "dist"),    cons(ts, vb, cons(ts, va, ts->l_nil))), ts->l_env), (L)5.0));
    r2_assert("dist2 vec2",   equ(eval(ts, cons(ts, atom(ts, "dist2"),   cons(ts, vb, cons(ts, va, ts->l_nil))), ts->l_env), (L)25.0));

    L n = eval(ts, cons(ts, atom(ts, "normalize"), cons(ts, va, ts->l_nil)), ts->l_env);
    r2_assert("normalize vec2[0] == 0.6", ts->tensor_heap[ord(n)].data[0] == 0.6f);
    r2_assert("normalize vec2[1] == 0.8", ts->tensor_heap[ord(n)].data[1] == 0.8f);
    return NULL;
}

static const char *test_tensor_fastpath_vec4(void)
{
    setup();
    float a[] = {1.f, 0.f, 0.f, 0.f};
    float b[] = {0.f, 1.f, 0.f, 0.f};
    L va = make_vec(4, a), vb = make_vec(4, b);

    r2_assert("dot vec4 orthogonal == 0", equ(eval(ts, cons(ts, atom(ts, "dot"), cons(ts, va, cons(ts, vb, ts->l_nil))), ts->l_env), (L)0.0));
    r2_assert("norm vec4 unit == 1",    equ(eval(ts, cons(ts, atom(ts, "norm"), cons(ts, va, ts->l_nil)), ts->l_env), (L)1.0));

    float c[] = {-1.f, 4.f, -9.f, 16.f};
    L vc = make_vec(4, c);
    L ab = eval(ts, cons(ts, atom(ts, "abs"),  cons(ts, vc, ts->l_nil)), ts->l_env);
    L sq = eval(ts, cons(ts, atom(ts, "sqrt"), cons(ts, vc, ts->l_nil)), ts->l_env);  /* sqrt of abs vals */
    r2_assert("abs vec4[0] == 1",  ts->tensor_heap[ord(ab)].data[0] == 1.f);
    r2_assert("abs vec4[2] == 9",  ts->tensor_heap[ord(ab)].data[2] == 9.f);

    float pos[] = {4.f, 9.f, 16.f, 25.f};
    L vp = make_vec(4, pos);
    L sr = eval(ts, cons(ts, atom(ts, "sqrt"), cons(ts, vp, ts->l_nil)), ts->l_env);
    r2_assert("sqrt vec4[0] == 2", ts->tensor_heap[ord(sr)].data[0] == 2.f);
    r2_assert("sqrt vec4[3] == 5", ts->tensor_heap[ord(sr)].data[3] == 5.f);
    (void)sq; (void)ab;
    return NULL;
}

/* -----------------------------------------------------------------------
   regression — multi-byte atoms used as values (not just function names)
   --------------------------------------------------------------------- */

static const char *test_utf8_atoms_as_values(void)
{
    setup();

    /* store a number under a multi-byte key, retrieve it */
    L def1 = cons(ts, atom(ts, "define"),
                  cons(ts, atom(ts, "\xE4\xBB\x96"), cons(ts, (L)3.0, ts->l_nil))); /* 他 */
    eval(ts, def1, ts->l_env);
    r2_assert("CJK atom as value",
              equ(eval(ts, atom(ts, "\xE4\xBB\x96"), ts->l_env), (L)3.0));

    /* use in arithmetic */
    L add = cons(ts, atom(ts, "+"),
                 cons(ts, atom(ts, "\xE4\xBB\x96"), cons(ts, (L)1.0, ts->l_nil)));
    r2_assert("CJK atom in arithmetic",
              equ(eval(ts, add, ts->l_env), (L)4.0));

    /* store under emoji, use in expression */
    L def2 = cons(ts, atom(ts, "define"),
                  cons(ts, atom(ts, "\xF0\x9F\x94\xA5"), cons(ts, (L)100.0, ts->l_nil))); /* 🔥 */
    eval(ts, def2, ts->l_env);
    L mul = cons(ts, atom(ts, "*"),
                 cons(ts, atom(ts, "\xF0\x9F\x94\xA5"), cons(ts, (L)2.0, ts->l_nil)));
    r2_assert("emoji atom in arithmetic",
              equ(eval(ts, mul, ts->l_env), (L)200.0));

    /* store a tensor under a multi-byte name */
    float d[] = {1.f, 2.f, 3.f};
    L v = make_vec(3, d);
    L def3 = cons(ts, atom(ts, "define"), cons(ts, atom(ts, "\xCF\x80\xCF\x80"), cons(ts, v, ts->l_nil))); /* ππ */
    eval(ts, def3, ts->l_env);
    L sh = eval(ts, cons(ts, atom(ts, "shape"), cons(ts, atom(ts, "\xCF\x80\xCF\x80"), ts->l_nil)), ts->l_env);
    r2_assert("multi-byte atom stores tensor",
              ts->tensor_heap[ord(sh)].data[0] == 3.f);

    return NULL;
}

/* -----------------------------------------------------------------------
   eval — undefined symbol returns L_ERR
   --------------------------------------------------------------------- */

static const char *test_eval_undefined(void)
{
    setup();
    r2_assert("unknown atom returns L_ERR",
              equ(eval(ts, atom(ts, "undefined-xyz"), ts->l_env), ts->l_err));
    return NULL;
}

/* -----------------------------------------------------------------------
   make-tensor primitive
   --------------------------------------------------------------------- */

static const char *test_make_tensor_scalars(void)
{
    setup();
    /* (make-tensor 1 2 3) - three scalars become a rank-1 vector */
    L expr = cons(ts, atom(ts, "make-tensor"), cons(ts, (L)1.0, cons(ts, (L)2.0, cons(ts, (L)3.0, ts->l_nil))));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("scalars: is TENS",    T(r) == TENS);
    r2_assert("scalars: rank == 1",  ts->tensor_heap[ord(r)].rank == 1);
    r2_assert("scalars: len == 3",   ts->tensor_heap[ord(r)].len == 3);
    r2_assert("scalars: data[0]==1", ts->tensor_heap[ord(r)].data[0] == 1.f);
    r2_assert("scalars: data[1]==2", ts->tensor_heap[ord(r)].data[1] == 2.f);
    r2_assert("scalars: data[2]==3", ts->tensor_heap[ord(r)].data[2] == 3.f);
    return NULL;
}

static const char *test_make_tensor_expr(void)
{
    setup();
    /* (make-tensor (+ 1 2) 4) - s-expression as a tensor element */
    L add  = cons(ts, atom(ts, "+"), cons(ts, (L)1.0, cons(ts, (L)2.0, ts->l_nil)));
    L expr = cons(ts, atom(ts, "make-tensor"), cons(ts, add, cons(ts, (L)4.0, ts->l_nil)));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("expr: is TENS",    T(r) == TENS);
    r2_assert("expr: data[0]==3", ts->tensor_heap[ord(r)].data[0] == 3.f);
    r2_assert("expr: data[1]==4", ts->tensor_heap[ord(r)].data[1] == 4.f);
    return NULL;
}

static const char *test_make_tensor_var(void)
{
    setup();
    /* (define x 5) then (make-tensor x 2 3) - variable lookup inside tensor */
    L def  = cons(ts, atom(ts, "define"), cons(ts, atom(ts, "x"), cons(ts, (L)5.0, ts->l_nil)));
    eval(ts, def, ts->l_env);
    L expr = cons(ts, atom(ts, "make-tensor"), cons(ts, atom(ts, "x"), cons(ts, (L)2.0, cons(ts, (L)3.0, ts->l_nil))));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("var: is TENS",    T(r) == TENS);
    r2_assert("var: data[0]==5", ts->tensor_heap[ord(r)].data[0] == 5.f);
    r2_assert("var: data[1]==2", ts->tensor_heap[ord(r)].data[1] == 2.f);
    r2_assert("var: data[2]==3", ts->tensor_heap[ord(r)].data[2] == 3.f);
    return NULL;
}

static const char *test_make_tensor_stack(void)
{
    setup();
    /* (make-tensor (make-tensor 1 2) (make-tensor 3 4)) - stacks rows into [[1 2][3 4]] */
    L row0 = cons(ts, atom(ts, "make-tensor"), cons(ts, (L)1.0, cons(ts, (L)2.0, ts->l_nil)));
    L row1 = cons(ts, atom(ts, "make-tensor"), cons(ts, (L)3.0, cons(ts, (L)4.0, ts->l_nil)));
    L expr = cons(ts, atom(ts, "make-tensor"), cons(ts, row0, cons(ts, row1, ts->l_nil)));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("stack: is TENS",       T(r) == TENS);
    r2_assert("stack: rank == 2",     ts->tensor_heap[ord(r)].rank == 2);
    r2_assert("stack: shape[0] == 2", ts->tensor_heap[ord(r)].shape[0] == 2);
    r2_assert("stack: shape[1] == 2", ts->tensor_heap[ord(r)].shape[1] == 2);
    r2_assert("stack: data[0] == 1",  ts->tensor_heap[ord(r)].data[0] == 1.f);
    r2_assert("stack: data[3] == 4",  ts->tensor_heap[ord(r)].data[3] == 4.f);
    return NULL;
}

static const char *test_make_tensor_expr_in_matrix(void)
{
    setup();
    /* The motivating use case: define x=3, build [(+ 3 x) x] which should give [6 3] */
    L def  = cons(ts, atom(ts, "define"), cons(ts, atom(ts, "x"), cons(ts, (L)3.0, ts->l_nil)));
    eval(ts, def, ts->l_env);
    L add  = cons(ts, atom(ts, "+"), cons(ts, (L)3.0, cons(ts, atom(ts, "x"), ts->l_nil)));
    L expr = cons(ts, atom(ts, "make-tensor"), cons(ts, add, cons(ts, atom(ts, "x"), ts->l_nil)));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("mat-expr: is TENS",    T(r) == TENS);
    r2_assert("mat-expr: data[0]==6", ts->tensor_heap[ord(r)].data[0] == 6.f);
    r2_assert("mat-expr: data[1]==3", ts->tensor_heap[ord(r)].data[1] == 3.f);
    return NULL;
}

/* -----------------------------------------------------------------------
   homoiconicity -- tensor literals as code-as-data
   --------------------------------------------------------------------- */

static const char *test_eval_quoted_tensor(void)
{
    setup();
    /* (eval '[1 2 3]) - quote prevents evaluation, eval triggers it */
    L mt   = cons(ts, atom(ts, "make-tensor"), cons(ts, (L)1.0, cons(ts, (L)2.0, cons(ts, (L)3.0, ts->l_nil))));
    L expr = cons(ts, atom(ts, "eval"), cons(ts, cons(ts, atom(ts, "quote"), cons(ts, mt, ts->l_nil)), ts->l_nil));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("quoted eval: is TENS",    T(r) == TENS);
    r2_assert("quoted eval: data[0]==1", ts->tensor_heap[ord(r)].data[0] == 1.f);
    r2_assert("quoted eval: data[2]==3", ts->tensor_heap[ord(r)].data[2] == 3.f);
    return NULL;
}

static const char *test_define_code_eval_later(void)
{
    setup();
    /* (define code '[(+ 3 x) x]) then (define x 4) then (eval code)
       the expression is stored unevaluated and uses whatever x is at eval time */
    L mt     = cons(ts, atom(ts, "make-tensor"),
                    cons(ts, cons(ts, atom(ts, "+"), cons(ts, (L)3.0, cons(ts, atom(ts, "x"), ts->l_nil))),
                         cons(ts, atom(ts, "x"), ts->l_nil)));
    L def_code = cons(ts, atom(ts, "define"),
                      cons(ts, atom(ts, "code"), cons(ts, cons(ts, atom(ts, "quote"), cons(ts, mt, ts->l_nil)), ts->l_nil)));
    eval(ts, def_code, ts->l_env);
    L def_x = cons(ts, atom(ts, "define"), cons(ts, atom(ts, "x"), cons(ts, (L)4.0, ts->l_nil)));
    eval(ts, def_x, ts->l_env);
    L expr = cons(ts, atom(ts, "eval"), cons(ts, atom(ts, "code"), ts->l_nil));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("defts->l_erred: is TENS",    T(r) == TENS);
    r2_assert("defts->l_erred: data[0]==7", ts->tensor_heap[ord(r)].data[0] == 7.f);
    r2_assert("defts->l_erred: data[1]==4", ts->tensor_heap[ord(r)].data[1] == 4.f);
    return NULL;
}

static const char *test_lambda_tensor_body(void)
{
    setup();
    /* (define make-row (lambda (a b) [a b])) then (make-row 5 6) => [5 6] */
    L body = cons(ts, atom(ts, "make-tensor"), cons(ts, atom(ts, "a"), cons(ts, atom(ts, "b"), ts->l_nil)));
    L lam  = cons(ts, atom(ts, "lambda"),
                  cons(ts, cons(ts, atom(ts, "a"), cons(ts, atom(ts, "b"), ts->l_nil)),
                       cons(ts, body, ts->l_nil)));
    L def  = cons(ts, atom(ts, "define"), cons(ts, atom(ts, "make-row"), cons(ts, lam, ts->l_nil)));
    eval(ts, def, ts->l_env);
    L call = cons(ts, atom(ts, "make-row"), cons(ts, (L)5.0, cons(ts, (L)6.0, ts->l_nil)));
    L r = eval(ts, call, ts->l_env);
    r2_assert("lambda body: is TENS",    T(r) == TENS);
    r2_assert("lambda body: data[0]==5", ts->tensor_heap[ord(r)].data[0] == 5.f);
    r2_assert("lambda body: data[1]==6", ts->tensor_heap[ord(r)].data[1] == 6.f);
    return NULL;
}

/* -----------------------------------------------------------------------
   print primitive
   --------------------------------------------------------------------- */

static const char *test_print_returns_value(void)
{
    setup();
    /* (print 42) should return 42 */
    L expr = cons(ts, atom(ts, "print"), cons(ts, (L)42.0, ts->l_nil));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("print: returns numeric value", r == (L)42.0);
    return NULL;
}

static const char *test_print_returns_tensor(void)
{
    setup();
    /* (define v [1 2 3]) (print v) should return the tensor */
    L mk   = cons(ts, atom(ts, "make-tensor"),
                  cons(ts, (L)1.0, cons(ts, (L)2.0, cons(ts, (L)3.0, ts->l_nil))));
    L def  = cons(ts, atom(ts, "define"), cons(ts, atom(ts, "v"), cons(ts, mk, ts->l_nil)));
    eval(ts, def, ts->l_env);
    L expr = cons(ts, atom(ts, "print"), cons(ts, atom(ts, "v"), ts->l_nil));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("print tensor: is TENS",    T(r) == TENS);
    r2_assert("print tensor: data[0]==1", ts->tensor_heap[ord(r)].data[0] == 1.f);
    return NULL;
}

/* -----------------------------------------------------------------------
   set! — in-place mutation
   --------------------------------------------------------------------- */

static const char *test_set_bang_scalar(void)
{
    setup();
    /* (define x 1) (setq x 42) => x is now 42 */
    L def = cons(ts, atom(ts, "define"), cons(ts, atom(ts, "x"), cons(ts, (L)1.0, ts->l_nil)));
    eval(ts, def, ts->l_env);
    L set = cons(ts, atom(ts, "setq"), cons(ts, atom(ts, "x"), cons(ts, (L)42.0, ts->l_nil)));
    L ret = eval(ts, set, ts->l_env);
    r2_assert("setq returns new value",        equ(ret, (L)42.0));
    r2_assert("setq mutates binding",          equ(eval(ts, atom(ts, "x"), ts->l_env), (L)42.0));
    return NULL;
}

static const char *test_set_bang_tensor(void)
{
    setup();
    /* (define W [1 2 3]) (setq W [7 8 9]) => W is now [7 8 9] */
    float d1[] = {1.f, 2.f, 3.f};
    float d2[] = {7.f, 8.f, 9.f};
    L v1 = make_vec(3, d1);
    L v2 = make_vec(3, d2);
    L def = cons(ts, atom(ts, "define"), cons(ts, atom(ts, "W"), cons(ts, v1, ts->l_nil)));
    eval(ts, def, ts->l_env);
    L set = cons(ts, atom(ts, "setq"), cons(ts, atom(ts, "W"), cons(ts, v2, ts->l_nil)));
    eval(ts, set, ts->l_env);
    L val = eval(ts, atom(ts, "W"), ts->l_env);
    r2_assert("setq tensor: is TENS",     T(val) == TENS);
    r2_assert("setq tensor: data[0]==7",  ts->tensor_heap[ord(val)].data[0] == 7.f);
    r2_assert("setq tensor: data[2]==9",  ts->tensor_heap[ord(val)].data[2] == 9.f);
    return NULL;
}

static const char *test_set_bang_unbound_is_l_err(void)
{
    setup();
    /* (setq undefined-var 1) => L_ERR (not defined) */
    L set = cons(ts, atom(ts, "setq"), cons(ts, atom(ts, "no-such-var"), cons(ts, (L)1.0, ts->l_nil)));
    r2_assert("setq unbound returns L_ERR", equ(eval(ts, set, ts->l_env), ts->l_err));
    return NULL;
}

static const char *test_set_bang_no_shadow(void)
{
    setup();
    /* define once, setq twice — env must not grow with shadow bindings */
    L def = cons(ts, atom(ts, "define"), cons(ts, atom(ts, "n"), cons(ts, (L)0.0, ts->l_nil)));
    eval(ts, def, ts->l_env);
    L env_after_define = ts->l_env;
    L set1 = cons(ts, atom(ts, "setq"), cons(ts, atom(ts, "n"), cons(ts, (L)1.0, ts->l_nil)));
    eval(ts, set1, ts->l_env);
    L set2 = cons(ts, atom(ts, "setq"), cons(ts, atom(ts, "n"), cons(ts, (L)2.0, ts->l_nil)));
    eval(ts, set2, ts->l_env);
    r2_assert("setq does not grow env",  equ(ts->l_env, env_after_define));
    r2_assert("n is 2 after two setqs",  equ(eval(ts, atom(ts, "n"), ts->l_env), (L)2.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   rank — scalars and rank-0 tensors
   --------------------------------------------------------------------- */

static const char *test_rank_scalar_number(void)
{
    setup();
    /* (rank 4) => 0  (a plain number is a rank-0 scalar) */
    L expr = cons(ts, atom(ts, "rank"), cons(ts, (L)4.0, ts->l_nil));
    r2_assert("rank of plain number is 0", equ(eval(ts, expr, ts->l_env), (L)0.0));
    return NULL;
}

static const char *test_rank1_single_tensor(void)
{
    setup();
    /* (make-tensor 5) => rank-1 vector with one element, shape [1] */
    L expr = cons(ts, atom(ts, "make-tensor"), cons(ts, (L)5.0, ts->l_nil));
    L r = eval(ts, expr, ts->l_env);
    r2_assert("single-elem tensor is TENS",       T(r) == TENS);
    r2_assert("single-elem tensor rank == 1",     ts->tensor_heap[ord(r)].rank == 1);
    r2_assert("single-elem tensor shape[0] == 1", ts->tensor_heap[ord(r)].shape[0] == 1);
    r2_assert("single-elem tensor data[0]==5",    ts->tensor_heap[ord(r)].data[0] == 5.f);
    /* (rank (make-tensor 5)) => 1 */
    L rank_expr = cons(ts, atom(ts, "rank"), cons(ts, expr, ts->l_nil));
    r2_assert("(rank [5]) == 1", equ(eval(ts, rank_expr, ts->l_env), (L)1.0));
    return NULL;
}

static const char *test_str_tag_reserved(void)
{
    setup();
    r2_assert("STR tag is 0x7ffe", STR == 0x7ffe);
    r2_assert("STR tag distinct from TENS", STR != TENS);
    return NULL;
}

/* Helper: parse one expression from a string and return the result of eval. */
static L parse_eval(const char *src)
{
    char buf_copy[256];
    snprintf(buf_copy, sizeof(buf_copy), "%s", src);
    FILE *f = fmemopen(buf_copy, strlen(buf_copy), "r");
    ts->input_stream = f;
    ts->see = ' ';
    L expr = Read(ts);
    fclose(f);
    ts->input_stream = NULL;
    return eval(ts, expr, ts->l_env);
}

static const char *test_scan_hash_t(void)
{
    setup();
    /* #t must be tokenized as a single atom, not split into '#' and 't' */
    L result = parse_eval("#t");
    r2_assert("scan #t is atom tru", equ(result, ts->l_tru));
    return NULL;
}

static const char *test_scan_cond_hash_t(void)
{
    setup();
    /* (cond (#t 42)) — the #t branch must be reachable */
    L result = parse_eval("(cond (#t 42))");
    r2_assert("(cond (#t 42)) == 42", equ(result, (L)42.0));
    return NULL;
}

static const char *test_scan_shebang(void)
{
    setup();
    /* #! at the start of a line must be skipped as a shebang comment */
    L result = parse_eval("#!/usr/bin/env basis\n99");
    r2_assert("shebang skipped, reads 99", equ(result, (L)99.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   Multi-instance isolation (lisp_state_t)
   --------------------------------------------------------------------- */

/* Helper: create a fully initialised interpreter instance. */
static lisp_state_t *make_instance(void)
{
    II i;
    lisp_state_t *s = lisp_state_new();
    s->l_nil = box(NIL, 0);
    s->l_err = atom(s, "L_ERR");
    s->l_tru = atom(s, "#t");
    s->l_env = pair(s, s->l_tru, s->l_tru, s->l_nil);
    register_tensor_prims(s);
    register_runtime_prims(s);
    for (i = 0; s->prim[i].s; i++)
        s->l_env = pair(s, atom(s, s->prim[i].s), box(PRIM, i), s->l_env);
    return s;
}

/* Helper: eval a string expression in an arbitrary instance (not ts). */
static L instance_eval(lisp_state_t *s, const char *src)
{
    char buf_copy[256];
    snprintf(buf_copy, sizeof(buf_copy), "%s", src);
    FILE *f = fmemopen(buf_copy, strlen(buf_copy), "r");
    s->input_stream = f;
    s->see = ' ';
    L expr = Read(s);
    fclose(f);
    s->input_stream = NULL;
    return eval(s, expr, s->l_env);
}

static const char *test_multiinstance_new_initial_state(void)
{
    lisp_state_t *s = lisp_state_new();
    r2_assert("hp starts at 0",  s->hp == 0);
    r2_assert("sp starts at N",  s->sp == N);
    r2_assert("th starts at 0",  s->th == 0);
    r2_assert("see starts at space", s->see == ' ');
    lisp_state_free(s);
    return NULL;
}

static const char *test_multiinstance_env_isolation(void)
{
    /* define x=1 in instance A; instance B must not see x */
    lisp_state_t *a = make_instance();
    lisp_state_t *b = make_instance();

    /* define x = 1 in a */
    a->l_env = pair(a, atom(a, "x"), (L)1.0, a->l_env);
    L x_in_a = eval(a, atom(a, "x"), a->l_env);
    r2_assert("x is 1 in instance a", equ(x_in_a, (L)1.0));

    /* x should not exist in b — eval returns l_err */
    L x_in_b = eval(b, atom(b, "x"), b->l_env);
    r2_assert("x is l_err in instance b", equ(x_in_b, b->l_err));

    lisp_state_free(a);
    lisp_state_free(b);
    return NULL;
}

static const char *test_multiinstance_define_no_cross_pollution(void)
{
    /* (define answer 42) in A must not affect B, and B can define its own */
    lisp_state_t *a = make_instance();
    lisp_state_t *b = make_instance();

    instance_eval(a, "(define answer 42)");
    instance_eval(b, "(define answer 99)");

    L va = instance_eval(a, "answer");
    L vb = instance_eval(b, "answer");

    r2_assert("answer is 42 in a", equ(va, (L)42.0));
    r2_assert("answer is 99 in b", equ(vb, (L)99.0));
    r2_assert("a and b disagree",  !equ(va, vb));

    lisp_state_free(a);
    lisp_state_free(b);
    return NULL;
}

static const char *test_multiinstance_tensor_isolation(void)
{
    /* Tensors allocated in A must not be visible in B's tensor_heap */
    lisp_state_t *a = make_instance();
    lisp_state_t *b = make_instance();

    instance_eval(a, "(define v [1 2 3])");
    II th_a = a->th;
    II th_b = b->th;

    r2_assert("tensor allocated in a", th_a > 0);
    r2_assert("b tensor heap unaffected", th_b == 0);

    lisp_state_free(a);
    lisp_state_free(b);
    return NULL;
}

static const char *test_multiinstance_gc_isolation(void)
{
    /* gc on instance A must not touch instance B's tensor heap */
    lisp_state_t *a = make_instance();
    lisp_state_t *b = make_instance();

    instance_eval(a, "(define v [1 2 3])");
    instance_eval(b, "(define w [4 5 6])");

    II th_b_before = b->th;
    gc(a);   /* GC only instance a */
    II th_b_after = b->th;

    r2_assert("b tensor heap unchanged after gc(a)", th_b_before == th_b_after);

    lisp_state_free(a);
    lisp_state_free(b);
    return NULL;
}

/* -----------------------------------------------------------------------
   Test runner
   --------------------------------------------------------------------- */

static const char *all_tests(void)
{
    r2_run_test(test_box_tags);
    r2_run_test(test_box_ord_roundtrip);
    r2_run_test(test_equ);
    r2_run_test(test_atom_interning);
    r2_run_test(test_atom_utf8);
    r2_run_test(test_cons_car_cdr);
    r2_run_test(test_cons_nested);
    r2_run_test(test_car_cdr_non_pair);
    r2_run_test(test_is_nil);
    r2_run_test(test_eval_numbers);
    r2_run_test(test_eval_add);
    r2_run_test(test_eval_sub);
    r2_run_test(test_eval_mul);
    r2_run_test(test_eval_div);
    r2_run_test(test_eval_int);
    r2_run_test(test_eval_lt);
    r2_run_test(test_eval_eq);
    r2_run_test(test_eval_not);
    r2_run_test(test_eval_and);
    r2_run_test(test_eval_or);
    r2_run_test(test_eval_quote);
    r2_run_test(test_eval_if);
    r2_run_test(test_eval_cond);
    r2_run_test(test_eval_pair);
    r2_run_test(test_eval_cons_car_cdr);
    r2_run_test(test_eval_lambda);
    r2_run_test(test_eval_lambda_multi_arg);
    r2_run_test(test_eval_closure_captures);
    r2_run_test(test_eval_define);
    r2_run_test(test_eval_define_lambda);
    r2_run_test(test_eval_leta);
    r2_run_test(test_eval_leta_sequential);
    r2_run_test(test_eval_recursion);
    r2_run_test(test_eval_utf8_atoms);
    r2_run_test(test_eval_undefined);
    r2_run_test(test_tens_tag);
    r2_run_test(test_tensor_predicate);
    r2_run_test(test_tensor_shape_rank_vec);
    r2_run_test(test_tensor_shape_rank_mat);
    r2_run_test(test_tensor_slice_vec);
    r2_run_test(test_tensor_slice_mat);
    r2_run_test(test_tensor_add);
    r2_run_test(test_tensor_sub);
    r2_run_test(test_tensor_mul);
    r2_run_test(test_tensor_div);
    r2_run_test(test_tensor_scalar_broadcast);
    r2_run_test(test_tensor_mat_add);
    r2_run_test(test_tensor_define);
    r2_run_test(test_tensor_matmul_square);
    r2_run_test(test_tensor_matmul_rect);
    r2_run_test(test_tensor_matmul_matvec);
    r2_run_test(test_tensor_matmul_vecmat);
    r2_run_test(test_tensor_transpose);
    r2_run_test(test_tensor_abs);
    r2_run_test(test_tensor_sqrt);
    r2_run_test(test_tensor_normalize);
    r2_run_test(test_tensor_pow);
    r2_run_test(test_tensor_zero);
    r2_run_test(test_tensor_dot);
    r2_run_test(test_tensor_length);
    r2_run_test(test_tensor_dist);
    r2_run_test(test_tensor_veq);
    r2_run_test(test_tensor_head_tail_vec);
    r2_run_test(test_tensor_head_tail_mat);
    r2_run_test(test_tensor_fastpath_vec2);
    r2_run_test(test_tensor_fastpath_vec4);
    r2_run_test(test_utf8_atoms_as_values);
    r2_run_test(test_make_tensor_scalars);
    r2_run_test(test_make_tensor_expr);
    r2_run_test(test_make_tensor_var);
    r2_run_test(test_make_tensor_stack);
    r2_run_test(test_make_tensor_expr_in_matrix);
    r2_run_test(test_eval_quoted_tensor);
    r2_run_test(test_define_code_eval_later);
    r2_run_test(test_lambda_tensor_body);
    r2_run_test(test_print_returns_value);
    r2_run_test(test_print_returns_tensor);
    r2_run_test(test_set_bang_scalar);
    r2_run_test(test_set_bang_tensor);
    r2_run_test(test_set_bang_unbound_is_l_err);
    r2_run_test(test_set_bang_no_shadow);
    r2_run_test(test_rank_scalar_number);
    r2_run_test(test_rank1_single_tensor);
    r2_run_test(test_str_tag_reserved);
    r2_run_test(test_scan_hash_t);
    r2_run_test(test_scan_cond_hash_t);
    r2_run_test(test_scan_shebang);
    r2_run_test(test_multiinstance_new_initial_state);
    r2_run_test(test_multiinstance_env_isolation);
    r2_run_test(test_multiinstance_define_no_cross_pollution);
    r2_run_test(test_multiinstance_tensor_isolation);
    r2_run_test(test_multiinstance_gc_isolation);
    return NULL;
}

int main(void)
{
    const char *result = all_tests();
    if (result)
        printf("FAILED: %s\n", result);
    else
        printf("ALL TESTS PASSED (%d tests)\n", r2_tests_run);
    return result != NULL;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "r2_unit.h"
#include "tinylisp.h"

int r2_tests_run = 0;

/* Reset the interpreter to a clean state before each test. Calling this at
   the top of every test ensures no state leaks between tests. */
static void setup(void)
{
    I i;
    hp  = 0;
    sp  = N;
    nil = box(NIL, 0);
    err = atom("ERR");
    tru = atom("#t");
    env = pair(tru, tru, nil);
    for (i = 0; prim[i].s; i++)
        env = pair(atom(prim[i].s), box(PRIM, i), env);
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
    L a1 = atom("hello");
    L a2 = atom("hello");
    L a3 = atom("world");
    r2_assert("same name interns to same value", equ(a1, a2));
    r2_assert("different names differ",         !equ(a1, a3));
    r2_assert("atom has ATOM tag",               T(a1) == ATOM);
    return NULL;
}

static const char *test_atom_utf8(void)
{
    setup();
    L fire1 = atom("\xF0\x9F\x94\xA5");   /* 🔥 */
    L fire2 = atom("\xF0\x9F\x94\xA5");   /* 🔥 same */
    L water = atom("\xF0\x9F\x92\xA7");   /* 💧 different */
    L pi    = atom("\xCE\xBB");            /* λ  two-byte */
    r2_assert("emoji interns identically",      equ(fire1, fire2));
    r2_assert("different emoji differ",        !equ(fire1, water));
    r2_assert("emoji has ATOM tag",             T(fire1) == ATOM);
    r2_assert("two-byte rune has ATOM tag",     T(pi) == ATOM);
    r2_assert("two-byte rune interns same",     equ(pi, atom("\xCE\xBB")));
    return NULL;
}

/* -----------------------------------------------------------------------
   cons / car / cdr
   --------------------------------------------------------------------- */

static const char *test_cons_car_cdr(void)
{
    setup();
    L p = cons((L)1.0, (L)2.0);
    r2_assert("cons has CONS tag",    T(p) == CONS);
    r2_assert("car returns first",    equ(car(p), (L)1.0));
    r2_assert("cdr returns second",   equ(cdr(p), (L)2.0));
    return NULL;
}

static const char *test_cons_nested(void)
{
    setup();
    /* (1 2 3) as a proper list */
    L lst = cons((L)1.0, cons((L)2.0, cons((L)3.0, nil)));
    r2_assert("car of list",          equ(car(lst), (L)1.0));
    r2_assert("cadr of list",         equ(car(cdr(lst)), (L)2.0));
    r2_assert("caddr of list",        equ(car(cdr(cdr(lst))), (L)3.0));
    r2_assert("cdddr of list is nil", is_nil(cdr(cdr(cdr(lst)))));
    return NULL;
}

static const char *test_car_cdr_non_pair(void)
{
    setup();
    r2_assert("car of number is ERR", equ(car((L)42.0), err));
    r2_assert("cdr of number is ERR", equ(cdr((L)42.0), err));
    r2_assert("car of nil is ERR",    equ(car(nil), err));
    return NULL;
}

/* -----------------------------------------------------------------------
   is_nil
   --------------------------------------------------------------------- */

static const char *test_is_nil(void)
{
    setup();
    r2_assert("nil is nil",                is_nil(nil));
    r2_assert("tru is not nil",           !is_nil(tru));
    r2_assert("number is not nil",        !is_nil((L)42.0));
    r2_assert("zero is not nil",          !is_nil((L)0.0));
    r2_assert("cons cell is not nil",     !is_nil(cons(nil, nil)));
    r2_assert("ERR atom is not nil",      !is_nil(err));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — self-evaluating forms
   --------------------------------------------------------------------- */

static const char *test_eval_numbers(void)
{
    setup();
    r2_assert("positive number",  equ(eval((L)42.0,  env), (L)42.0));
    r2_assert("zero",             equ(eval((L)0.0,   env), (L)0.0));
    r2_assert("negative number",  equ(eval((L)-7.0,  env), (L)-7.0));
    r2_assert("float",            equ(eval((L)3.14,  env), (L)3.14));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — arithmetic primitives
   --------------------------------------------------------------------- */

static const char *test_eval_add(void)
{
    setup();
    /* (+ 1 2) => 3 */
    L e1 = cons(atom("+"), cons((L)1.0, cons((L)2.0, nil)));
    r2_assert("(+ 1 2) == 3", equ(eval(e1, env), (L)3.0));

    /* (+ 1 2 3) => 6 */
    L e2 = cons(atom("+"), cons((L)1.0, cons((L)2.0, cons((L)3.0, nil))));
    r2_assert("(+ 1 2 3) == 6", equ(eval(e2, env), (L)6.0));

    /* (+ 0 0) => 0 */
    L e3 = cons(atom("+"), cons((L)0.0, cons((L)0.0, nil)));
    r2_assert("(+ 0 0) == 0", equ(eval(e3, env), (L)0.0));
    return NULL;
}

static const char *test_eval_sub(void)
{
    setup();
    L e1 = cons(atom("-"), cons((L)10.0, cons((L)3.0, nil)));
    r2_assert("(- 10 3) == 7", equ(eval(e1, env), (L)7.0));

    /* (- 10 3 2) => 5 */
    L e2 = cons(atom("-"), cons((L)10.0, cons((L)3.0, cons((L)2.0, nil))));
    r2_assert("(- 10 3 2) == 5", equ(eval(e2, env), (L)5.0));
    return NULL;
}

static const char *test_eval_mul(void)
{
    setup();
    L e1 = cons(atom("*"), cons((L)6.0,  cons((L)7.0, nil)));
    r2_assert("(* 6 7) == 42",  equ(eval(e1, env), (L)42.0));

    L e2 = cons(atom("*"), cons((L)2.0, cons((L)3.0, cons((L)4.0, nil))));
    r2_assert("(* 2 3 4) == 24", equ(eval(e2, env), (L)24.0));
    return NULL;
}

static const char *test_eval_div(void)
{
    setup();
    L e1 = cons(atom("/"), cons((L)10.0, cons((L)2.0, nil)));
    r2_assert("(/ 10 2) == 5", equ(eval(e1, env), (L)5.0));

    /* (/ 100 2 5) => 10 */
    L e2 = cons(atom("/"), cons((L)100.0, cons((L)2.0, cons((L)5.0, nil))));
    r2_assert("(/ 100 2 5) == 10", equ(eval(e2, env), (L)10.0));
    return NULL;
}

static const char *test_eval_int(void)
{
    setup();
    L e1 = cons(atom("int"), cons((L)3.9,  nil));
    r2_assert("(int 3.9) == 3",   equ(eval(e1, env), (L)3.0));

    L e2 = cons(atom("int"), cons((L)-2.7, nil));
    r2_assert("(int -2.7) == -2", equ(eval(e2, env), (L)-2.0));

    L e3 = cons(atom("int"), cons((L)5.0,  nil));
    r2_assert("(int 5.0) == 5",   equ(eval(e3, env), (L)5.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — comparison and equality
   --------------------------------------------------------------------- */

static const char *test_eval_lt(void)
{
    setup();
    L lt_t = cons(atom("<"), cons((L)1.0, cons((L)2.0, nil)));
    L lt_f = cons(atom("<"), cons((L)2.0, cons((L)1.0, nil)));
    L lt_e = cons(atom("<"), cons((L)2.0, cons((L)2.0, nil)));
    r2_assert("(< 1 2) is tru",   equ(eval(lt_t, env), tru));
    r2_assert("(< 2 1) is nil",   equ(eval(lt_f, env), nil));
    r2_assert("(< 2 2) is nil",   equ(eval(lt_e, env), nil));
    return NULL;
}

static const char *test_eval_eq(void)
{
    setup();
    L eq_t = cons(atom("eq?"), cons((L)42.0, cons((L)42.0, nil)));
    L eq_f = cons(atom("eq?"), cons((L)1.0,  cons((L)2.0,  nil)));
    r2_assert("(eq? 42 42) is tru", equ(eval(eq_t, env), tru));
    r2_assert("(eq? 1 2) is nil",   equ(eval(eq_f, env), nil));

    /* atoms compare by identity */
    L sym = atom("foo");
    L eq_a = cons(atom("eq?"),
                  cons(cons(atom("quote"), cons(sym, nil)),
                       cons(cons(atom("quote"), cons(sym, nil)), nil)));
    r2_assert("(eq? 'foo 'foo) is tru", equ(eval(eq_a, env), tru));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — boolean: not / and / or
   --------------------------------------------------------------------- */

static const char *test_eval_not(void)
{
    setup();
    L not_nil = cons(atom("not"), cons(nil, nil));
    L not_tru = cons(atom("not"), cons(tru, nil));
    L not_num = cons(atom("not"), cons((L)42.0, nil));
    r2_assert("(not ()) is tru",  equ(eval(not_nil, env), tru));
    r2_assert("(not #t) is nil",  equ(eval(not_tru, env), nil));
    r2_assert("(not 42) is nil",  equ(eval(not_num, env), nil));
    return NULL;
}

static const char *test_eval_and(void)
{
    setup();
    L tt = cons(atom("and"), cons(tru, cons(tru,      nil)));
    L tf = cons(atom("and"), cons(tru, cons(nil,      nil)));
    L ff = cons(atom("and"), cons(nil, cons(nil,      nil)));
    r2_assert("(and #t #t) truthy", !is_nil(eval(tt, env)));
    r2_assert("(and #t ()) is nil",  is_nil(eval(tf, env)));
    r2_assert("(and () ()) is nil",  is_nil(eval(ff, env)));
    return NULL;
}

static const char *test_eval_or(void)
{
    setup();
    L ff = cons(atom("or"), cons(nil, cons(nil, nil)));
    L tf = cons(atom("or"), cons(tru, cons(nil, nil)));
    L ft = cons(atom("or"), cons(nil, cons(tru, nil)));
    r2_assert("(or () ()) is nil",   is_nil(eval(ff, env)));
    r2_assert("(or #t ()) is truthy", !is_nil(eval(tf, env)));
    r2_assert("(or () #t) is truthy", !is_nil(eval(ft, env)));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — quote
   --------------------------------------------------------------------- */

static const char *test_eval_quote(void)
{
    setup();
    L sym  = atom("hello");
    L expr = cons(atom("quote"), cons(sym, nil));
    r2_assert("(quote hello) returns atom unevaluated", equ(eval(expr, env), sym));

    /* quoted list is not evaluated */
    L lst      = cons((L)1.0, cons((L)2.0, nil));
    L qlst     = cons(atom("quote"), cons(lst, nil));
    L result   = eval(qlst, env);
    r2_assert("(quote (1 2)) is a pair",    T(result) == CONS);
    r2_assert("car of quoted list is 1",    equ(car(result), (L)1.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — if / cond
   --------------------------------------------------------------------- */

static const char *test_eval_if(void)
{
    setup();
    /* (if #t 1 2) => 1 */
    L if_t = cons(atom("if"), cons(tru, cons((L)1.0, cons((L)2.0, nil))));
    r2_assert("(if #t 1 2) == 1", equ(eval(if_t, env), (L)1.0));

    /* (if () 1 2) => 2 */
    L if_f = cons(atom("if"), cons(nil, cons((L)1.0, cons((L)2.0, nil))));
    r2_assert("(if () 1 2) == 2", equ(eval(if_f, env), (L)2.0));
    return NULL;
}

static const char *test_eval_cond(void)
{
    setup();
    /* (cond (() 1) (#t 2)) => 2 */
    L c1   = cons(nil, cons((L)1.0, nil));
    L c2   = cons(tru, cons((L)2.0, nil));
    L expr = cons(atom("cond"), cons(c1, cons(c2, nil)));
    r2_assert("(cond (() 1)(#t 2)) == 2", equ(eval(expr, env), (L)2.0));

    /* (cond (#t 99)) => 99 */
    L c3    = cons(tru, cons((L)99.0, nil));
    L expr2 = cons(atom("cond"), cons(c3, nil));
    r2_assert("(cond (#t 99)) == 99", equ(eval(expr2, env), (L)99.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — pair?
   --------------------------------------------------------------------- */

static const char *test_eval_pair(void)
{
    setup();
    /* (pair? '(1 . 2)) => #t */
    L p       = cons((L)1.0, (L)2.0);
    L qp      = cons(atom("quote"), cons(p, nil));
    L is_pair = cons(atom("pair?"), cons(qp, nil));
    r2_assert("(pair? '(1 . 2)) is tru", equ(eval(is_pair, env), tru));

    /* (pair? 42) => () */
    L not_pair = cons(atom("pair?"), cons((L)42.0, nil));
    r2_assert("(pair? 42) is nil", equ(eval(not_pair, env), nil));

    /* (pair? ()) => () */
    L qnil      = cons(atom("quote"), cons(nil, nil));
    L nil_pair  = cons(atom("pair?"), cons(qnil, nil));
    r2_assert("(pair? '()) is nil", equ(eval(nil_pair, env), nil));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — cons / car / cdr as Lisp primitives
   --------------------------------------------------------------------- */

static const char *test_eval_cons_car_cdr(void)
{
    setup();
    /* (car (cons 10 20)) => 10 */
    L inner    = cons(atom("cons"), cons((L)10.0, cons((L)20.0, nil)));
    L car_expr = cons(atom("car"),  cons(inner,   nil));
    r2_assert("(car (cons 10 20)) == 10", equ(eval(car_expr, env), (L)10.0));

    /* (cdr (cons 10 20)) => 20 */
    L inner2   = cons(atom("cons"), cons((L)10.0, cons((L)20.0, nil)));
    L cdr_expr = cons(atom("cdr"),  cons(inner2,  nil));
    r2_assert("(cdr (cons 10 20)) == 20", equ(eval(cdr_expr, env), (L)20.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — lambda and closures
   --------------------------------------------------------------------- */

static const char *test_eval_lambda(void)
{
    setup();
    /* ((lambda (x) (* x x)) 5) => 25 */
    L body  = cons(atom("*"), cons(atom("x"), cons(atom("x"), nil)));
    L lam   = cons(atom("lambda"), cons(cons(atom("x"), nil), cons(body, nil)));
    L call  = cons(lam, cons((L)5.0, nil));
    r2_assert("((lambda (x) (* x x)) 5) == 25", equ(eval(call, env), (L)25.0));
    return NULL;
}

static const char *test_eval_lambda_multi_arg(void)
{
    setup();
    /* ((lambda (x y) (+ x y)) 3 4) => 7 */
    L body  = cons(atom("+"), cons(atom("x"), cons(atom("y"), nil)));
    L args  = cons(atom("x"), cons(atom("y"), nil));
    L lam   = cons(atom("lambda"), cons(args, cons(body, nil)));
    L call  = cons(lam, cons((L)3.0, cons((L)4.0, nil)));
    r2_assert("((lambda (x y) (+ x y)) 3 4) == 7", equ(eval(call, env), (L)7.0));
    return NULL;
}

static const char *test_eval_closure_captures(void)
{
    setup();
    /* ((lambda (x) (lambda (y) (+ x y))) 10) => closure, then apply to 5 => 15 */
    L inner_body = cons(atom("+"), cons(atom("x"), cons(atom("y"), nil)));
    L inner_lam  = cons(atom("lambda"), cons(cons(atom("y"), nil), cons(inner_body, nil)));
    L outer_lam  = cons(atom("lambda"), cons(cons(atom("x"), nil), cons(inner_lam, nil)));
    L outer_call = cons(outer_lam, cons((L)10.0, nil));
    L adder      = eval(outer_call, env);
    r2_assert("outer returns a closure", T(adder) == CLOS);
    L inner_call = cons(adder, cons((L)5.0, nil));  /* can't use eval directly on cons(adder,...) */
    /* apply the closure */
    L result = apply(adder, cons((L)5.0, nil), env);
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
    L def = cons(atom("define"), cons(atom("answer"), cons((L)42.0, nil)));
    eval(def, env);
    r2_assert("defined value resolves", equ(eval(atom("answer"), env), (L)42.0));
    return NULL;
}

static const char *test_eval_define_lambda(void)
{
    setup();
    /* (define sq (lambda (x) (* x x))) then (sq 9) => 81 */
    L body   = cons(atom("*"), cons(atom("x"), cons(atom("x"), nil)));
    L lam    = cons(atom("lambda"), cons(cons(atom("x"), nil), cons(body, nil)));
    L def    = cons(atom("define"), cons(atom("sq"), cons(lam, nil)));
    eval(def, env);
    L call   = cons(atom("sq"), cons((L)9.0, nil));
    r2_assert("(sq 9) == 81 after define", equ(eval(call, env), (L)81.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — let*
   --------------------------------------------------------------------- */

static const char *test_eval_leta(void)
{
    setup();
    /* (let* (x 3) (y 4) (+ x y)) => 7 */
    L body  = cons(atom("+"), cons(atom("x"), cons(atom("y"), nil)));
    L b2    = cons(cons(atom("y"), cons((L)4.0, nil)), cons(body, nil));
    L b1    = cons(cons(atom("x"), cons((L)3.0, nil)), b2);
    L expr  = cons(atom("let*"), b1);
    r2_assert("(let* (x 3)(y 4)(+ x y)) == 7", equ(eval(expr, env), (L)7.0));
    return NULL;
}

static const char *test_eval_leta_sequential(void)
{
    setup();
    /* (let* (x 2) (y (* x 3)) y) => 6   y depends on x */
    L y_body = cons(atom("*"), cons(atom("x"), cons((L)3.0, nil)));
    L b2     = cons(cons(atom("y"), cons(y_body, nil)), cons(atom("y"), nil));
    L b1     = cons(cons(atom("x"), cons((L)2.0, nil)), b2);
    L expr   = cons(atom("let*"), b1);
    r2_assert("(let* (x 2)(y (* x 3)) y) == 6", equ(eval(expr, env), (L)6.0));
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
    L rec_call = cons(atom("fact"),
                      cons(cons(atom("-"), cons(atom("n"), cons((L)1.0, nil))), nil));
    L mul_expr = cons(atom("*"), cons(atom("n"), cons(rec_call, nil)));
    L body   = cons(atom("if"),
                    cons(cons(atom("<"), cons(atom("n"), cons((L)2.0, nil))),
                         cons(base, cons(mul_expr, nil))));
    L lam    = cons(atom("lambda"), cons(cons(atom("n"), nil), cons(body, nil)));
    L def    = cons(atom("define"), cons(atom("fact"), cons(lam, nil)));
    eval(def, env);
    L call   = cons(atom("fact"), cons((L)5.0, nil));
    r2_assert("(fact 5) == 120", equ(eval(call, env), (L)120.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — UTF-8 atom names
   --------------------------------------------------------------------- */

static const char *test_eval_utf8_atoms(void)
{
    setup();

    /* (define 🔥 42) then 🔥 => 42 */
    L def1 = cons(atom("define"), cons(atom("\xF0\x9F\x94\xA5"), cons((L)42.0, nil)));
    eval(def1, env);
    r2_assert("emoji atom resolves",
              equ(eval(atom("\xF0\x9F\x94\xA5"), env), (L)42.0));

    /* (define λ (lambda (x) (* x 2))) then (λ 21) => 42 */
    L body = cons(atom("*"), cons(atom("x"), cons((L)2.0, nil)));
    L lam  = cons(atom("lambda"), cons(cons(atom("x"), nil), cons(body, nil)));
    L def2 = cons(atom("define"), cons(atom("\xCE\xBB"), cons(lam, nil)));
    eval(def2, env);
    L call = cons(atom("\xCE\xBB"), cons((L)21.0, nil));
    r2_assert("(λ 21) == 42", equ(eval(call, env), (L)42.0));

    /* Greek π as a value */
    L def3 = cons(atom("define"), cons(atom("\xCF\x80"), cons((L)3.14159, nil)));
    eval(def3, env);
    r2_assert("π resolves to 3.14159",
              equ(eval(atom("\xCF\x80"), env), (L)3.14159));
    return NULL;
}

/* -----------------------------------------------------------------------
   eval — undefined symbol returns ERR
   --------------------------------------------------------------------- */

static const char *test_eval_undefined(void)
{
    setup();
    r2_assert("unknown atom returns ERR",
              equ(eval(atom("undefined-xyz"), env), err));
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

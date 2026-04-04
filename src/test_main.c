#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "r2_unit.h"
#include "tinylisp.h"
#include "tinytensor.h"
#include "runtime.h"

int r2_tests_run = 0;

/* Reset the interpreter to a clean state before each test. Calling this at
   the top of every test ensures no state leaks between tests. */
static void setup(void)
{
    I i;
    hp  = 0;
    sp  = N;
    th  = 0;
    prim_count = CORE_PRIM_COUNT;  /* reset to core-only, then re-register */
    prim[CORE_PRIM_COUNT].s = 0;   /* restore sentinel */
    nil = box(NIL, 0);
    err = atom("ERR");
    tru = atom("#t");
    env = pair(tru, tru, nil);
    register_tensor_prims();
    register_runtime_prims();
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
   tensor — helpers
   --------------------------------------------------------------------- */

/* Build a TENS L value from a flat float array inline */
static L make_vec(I len, const float *data)
{
    I shape[1];
    shape[0] = len;
    return box(TENS, (I)(alloc_tensor(1, shape, len, data) - tensor_heap));
}

static L make_mat(I rows, I cols, const float *data)
{
    I shape[2];
    shape[0] = rows;
    shape[1] = cols;
    return box(TENS, (I)(alloc_tensor(2, shape, rows * cols, data) - tensor_heap));
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
    /* (tensor? v) => #t */
    L expr_t = cons(atom("tensor?"), cons(v, nil));
    r2_assert("(tensor? [1 2]) is tru",   equ(eval(expr_t, env), tru));
    /* (tensor? 42) => () */
    L expr_f = cons(atom("tensor?"), cons((L)42.0, nil));
    r2_assert("(tensor? 42) is nil",      equ(eval(expr_f, env), nil));
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
    L rank_expr = cons(atom("rank"), cons(v, nil));
    r2_assert("rank of vec is 1", equ(eval(rank_expr, env), (L)1.0));

    /* (shape v) => [3] */
    L shape_expr = cons(atom("shape"), cons(v, nil));
    L sh = eval(shape_expr, env);
    r2_assert("shape of vec is TENS",        T(sh) == TENS);
    r2_assert("shape[0] of [10 20 30] == 3", tensor_heap[ord(sh)].data[0] == 3.f);
    return NULL;
}

static const char *test_tensor_shape_rank_mat(void)
{
    setup();
    float d[] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
    L m = make_mat(2, 3, d);

    /* (rank m) => 2 */
    L rank_expr = cons(atom("rank"), cons(m, nil));
    r2_assert("rank of 2x3 mat is 2", equ(eval(rank_expr, env), (L)2.0));

    /* (shape m) => [2 3] */
    L shape_expr = cons(atom("shape"), cons(m, nil));
    L sh = eval(shape_expr, env);
    r2_assert("shape[0] of 2x3 == 2", tensor_heap[ord(sh)].data[0] == 2.f);
    r2_assert("shape[1] of 2x3 == 3", tensor_heap[ord(sh)].data[1] == 3.f);
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

    L s0 = cons(atom("slice"), cons(v, cons((L)0.0, nil)));
    L s1 = cons(atom("slice"), cons(v, cons((L)1.0, nil)));
    L s2 = cons(atom("slice"), cons(v, cons((L)2.0, nil)));
    r2_assert("slice 0 == 10", equ(eval(s0, env), (L)10.0));
    r2_assert("slice 1 == 20", equ(eval(s1, env), (L)20.0));
    r2_assert("slice 2 == 30", equ(eval(s2, env), (L)30.0));
    return NULL;
}

static const char *test_tensor_slice_mat(void)
{
    setup();
    /* [[1 2 3][4 5 6]] — slice 0 => [1 2 3], slice 1 => [4 5 6] */
    float d[] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
    L m = make_mat(2, 3, d);

    L row0_expr = cons(atom("slice"), cons(m, cons((L)0.0, nil)));
    L row0 = eval(row0_expr, env);
    r2_assert("row0 is TENS",      T(row0) == TENS);
    r2_assert("row0 len == 3",     tensor_heap[ord(row0)].len == 3);
    r2_assert("row0[0] == 1",      tensor_heap[ord(row0)].data[0] == 1.f);
    r2_assert("row0[2] == 3",      tensor_heap[ord(row0)].data[2] == 3.f);

    L row1_expr = cons(atom("slice"), cons(m, cons((L)1.0, nil)));
    L row1 = eval(row1_expr, env);
    r2_assert("row1[0] == 4",      tensor_heap[ord(row1)].data[0] == 4.f);
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

    L expr = cons(atom("+"), cons(va, cons(vb, nil)));
    L r = eval(expr, env);
    r2_assert("vec add is TENS",    T(r) == TENS);
    r2_assert("1+4 == 5",           tensor_heap[ord(r)].data[0] == 5.f);
    r2_assert("2+5 == 7",           tensor_heap[ord(r)].data[1] == 7.f);
    r2_assert("3+6 == 9",           tensor_heap[ord(r)].data[2] == 9.f);
    return NULL;
}

static const char *test_tensor_sub(void)
{
    setup();
    float a[] = {10.f, 20.f, 30.f};
    float b[] = {1.f,  2.f,  3.f};
    L va = make_vec(3, a);
    L vb = make_vec(3, b);

    L expr = cons(atom("-"), cons(va, cons(vb, nil)));
    L r = eval(expr, env);
    r2_assert("10-1 == 9",  tensor_heap[ord(r)].data[0] == 9.f);
    r2_assert("20-2 == 18", tensor_heap[ord(r)].data[1] == 18.f);
    r2_assert("30-3 == 27", tensor_heap[ord(r)].data[2] == 27.f);
    return NULL;
}

static const char *test_tensor_mul(void)
{
    setup();
    float a[] = {2.f, 3.f, 4.f};
    float b[] = {1.f, 2.f, 3.f};
    L va = make_vec(3, a);
    L vb = make_vec(3, b);

    L expr = cons(atom("*"), cons(va, cons(vb, nil)));
    L r = eval(expr, env);
    r2_assert("2*1 == 2",   tensor_heap[ord(r)].data[0] == 2.f);
    r2_assert("3*2 == 6",   tensor_heap[ord(r)].data[1] == 6.f);
    r2_assert("4*3 == 12",  tensor_heap[ord(r)].data[2] == 12.f);
    return NULL;
}

static const char *test_tensor_div(void)
{
    setup();
    float a[] = {10.f, 20.f, 30.f};
    float b[] = {2.f,  4.f,  5.f};
    L va = make_vec(3, a);
    L vb = make_vec(3, b);

    L expr = cons(atom("/"), cons(va, cons(vb, nil)));
    L r = eval(expr, env);
    r2_assert("10/2 == 5",  tensor_heap[ord(r)].data[0] == 5.f);
    r2_assert("20/4 == 5",  tensor_heap[ord(r)].data[1] == 5.f);
    r2_assert("30/5 == 6",  tensor_heap[ord(r)].data[2] == 6.f);
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

    L add_expr = cons(atom("+"), cons(v, cons((L)10.0, nil)));
    L ra = eval(add_expr, env);
    r2_assert("broadcast + [0] == 11", tensor_heap[ord(ra)].data[0] == 11.f);
    r2_assert("broadcast + [2] == 13", tensor_heap[ord(ra)].data[2] == 13.f);

    L mul_expr = cons(atom("*"), cons(v, cons((L)2.0, nil)));
    L rm = eval(mul_expr, env);
    r2_assert("broadcast * [0] == 2",  tensor_heap[ord(rm)].data[0] == 2.f);
    r2_assert("broadcast * [1] == 4",  tensor_heap[ord(rm)].data[1] == 4.f);
    r2_assert("broadcast * [2] == 6",  tensor_heap[ord(rm)].data[2] == 6.f);
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

    L expr = cons(atom("+"), cons(ma, cons(mb, nil)));
    L r = eval(expr, env);
    r2_assert("mat add is TENS",     T(r) == TENS);
    r2_assert("mat add rank == 2",   tensor_heap[ord(r)].rank == 2);
    r2_assert("mat [0,0] == 11",     tensor_heap[ord(r)].data[0] == 11.f);
    r2_assert("mat [1,1] == 44",     tensor_heap[ord(r)].data[3] == 44.f);
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
    L def = cons(atom("define"), cons(atom("pi-vec"), cons(v, nil)));
    eval(def, env);

    /* (shape pi-vec) => [5] */
    L shape_expr = cons(atom("shape"), cons(atom("pi-vec"), nil));
    L sh = eval(shape_expr, env);
    r2_assert("shape of pi-vec == [5]", tensor_heap[ord(sh)].data[0] == 5.f);

    /* (slice pi-vec 2) => 4 */
    L sl = cons(atom("slice"), cons(atom("pi-vec"), cons((L)2.0, nil)));
    r2_assert("slice 2 of pi-vec == 4", equ(eval(sl, env), (L)4.0));
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

    L expr = cons(atom("matmul"), cons(ma, cons(mb, nil)));
    L r = eval(expr, env);
    r2_assert("matmul result is TENS",   T(r) == TENS);
    r2_assert("matmul rank == 2",        tensor_heap[ord(r)].rank == 2);
    r2_assert("[0,0] == 19",             tensor_heap[ord(r)].data[0] == 19.f);
    r2_assert("[0,1] == 22",             tensor_heap[ord(r)].data[1] == 22.f);
    r2_assert("[1,0] == 43",             tensor_heap[ord(r)].data[2] == 43.f);
    r2_assert("[1,1] == 50",             tensor_heap[ord(r)].data[3] == 50.f);
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

    L expr = cons(atom("matmul"), cons(ma, cons(mb, nil)));
    L r = eval(expr, env);
    r2_assert("rect matmul shape[0] == 2", tensor_heap[ord(r)].shape[0] == 2);
    r2_assert("rect matmul shape[1] == 2", tensor_heap[ord(r)].shape[1] == 2);
    r2_assert("[0,0] == 58",               tensor_heap[ord(r)].data[0] == 58.f);
    r2_assert("[0,1] == 64",               tensor_heap[ord(r)].data[1] == 64.f);
    r2_assert("[1,0] == 139",              tensor_heap[ord(r)].data[2] == 139.f);
    r2_assert("[1,1] == 154",              tensor_heap[ord(r)].data[3] == 154.f);
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

    L expr = cons(atom("matmul"), cons(ma, cons(mv, nil)));
    L r = eval(expr, env);
    r2_assert("mat-vec result is TENS",  T(r) == TENS);
    r2_assert("mat-vec rank == 1",       tensor_heap[ord(r)].rank == 1);
    r2_assert("result[0] == 1",          tensor_heap[ord(r)].data[0] == 1.f);
    r2_assert("result[1] == 4",          tensor_heap[ord(r)].data[1] == 4.f);
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

    L expr = cons(atom("@"), cons(mv, cons(mb, nil)));
    L r = eval(expr, env);
    r2_assert("vec-mat result is TENS",  T(r) == TENS);
    r2_assert("vec-mat rank == 1",       tensor_heap[ord(r)].rank == 1);
    r2_assert("result[0] == 7",          tensor_heap[ord(r)].data[0] == 7.f);
    r2_assert("result[1] == 11",         tensor_heap[ord(r)].data[1] == 11.f);
    return NULL;
}

static const char *test_tensor_transpose(void)
{
    setup();
    /* transpose [[1 2 3][4 5 6]] (2x3) => [[1 4][2 5][3 6]] (3x2) */
    float a[] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
    L m = make_mat(2, 3, a);

    L expr = cons(atom("transpose"), cons(m, nil));
    L r = eval(expr, env);
    r2_assert("transpose is TENS",       T(r) == TENS);
    r2_assert("transpose shape[0] == 3", tensor_heap[ord(r)].shape[0] == 3);
    r2_assert("transpose shape[1] == 2", tensor_heap[ord(r)].shape[1] == 2);
    r2_assert("[0,0] == 1",              tensor_heap[ord(r)].data[0] == 1.f);
    r2_assert("[0,1] == 4",              tensor_heap[ord(r)].data[1] == 4.f);
    r2_assert("[1,0] == 2",              tensor_heap[ord(r)].data[2] == 2.f);
    r2_assert("[2,1] == 6",              tensor_heap[ord(r)].data[5] == 6.f);
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
    L r = eval(cons(atom("abs"), cons(v, nil)), env);
    r2_assert("abs[0] == 3", tensor_heap[ord(r)].data[0] == 3.f);
    r2_assert("abs[1] == 1", tensor_heap[ord(r)].data[1] == 1.f);
    r2_assert("abs[2] == 2", tensor_heap[ord(r)].data[2] == 2.f);
    return NULL;
}

static const char *test_tensor_sqrt(void)
{
    setup();
    float d[] = {4.f, 9.f, 16.f};
    L v = make_vec(3, d);
    L r = eval(cons(atom("sqrt"), cons(v, nil)), env);
    r2_assert("sqrt[0] == 2", tensor_heap[ord(r)].data[0] == 2.f);
    r2_assert("sqrt[1] == 3", tensor_heap[ord(r)].data[1] == 3.f);
    r2_assert("sqrt[2] == 4", tensor_heap[ord(r)].data[2] == 4.f);
    return NULL;
}

static const char *test_tensor_normalize(void)
{
    setup();
    float d[] = {3.f, 4.f};
    L v = make_vec(2, d);
    L r = eval(cons(atom("normalize"), cons(v, nil)), env);
    r2_assert("normalize[0] == 0.6", tensor_heap[ord(r)].data[0] == 0.6f);
    r2_assert("normalize[1] == 0.8", tensor_heap[ord(r)].data[1] == 0.8f);
    return NULL;
}

static const char *test_tensor_pow(void)
{
    setup();
    float d[] = {2.f, 3.f, 4.f};
    L v = make_vec(3, d);
    L r = eval(cons(atom("pow"), cons(v, cons((L)2.0, nil))), env);
    r2_assert("pow[0] == 4",  tensor_heap[ord(r)].data[0] == 4.f);
    r2_assert("pow[1] == 9",  tensor_heap[ord(r)].data[1] == 9.f);
    r2_assert("pow[2] == 16", tensor_heap[ord(r)].data[2] == 16.f);
    return NULL;
}

static const char *test_tensor_zero(void)
{
    setup();
    L r = eval(cons(atom("zero"), cons((L)4.0, nil)), env);
    r2_assert("zero is TENS",     T(r) == TENS);
    r2_assert("zero len == 4",    tensor_heap[ord(r)].len == 4);
    r2_assert("zero[0] == 0",     tensor_heap[ord(r)].data[0] == 0.f);
    r2_assert("zero[3] == 0",     tensor_heap[ord(r)].data[3] == 0.f);
    return NULL;
}

static const char *test_tensor_dot(void)
{
    setup();
    float a[] = {1.f, 2.f, 3.f};
    float b[] = {4.f, 5.f, 6.f};
    L va = make_vec(3, a), vb = make_vec(3, b);
    L r = eval(cons(atom("dot"), cons(va, cons(vb, nil))), env);
    r2_assert("dot [1 2 3].[4 5 6] == 32", equ(r, (L)32.0));
    return NULL;
}

static const char *test_tensor_length(void)
{
    setup();
    float d[] = {3.f, 4.f};
    L v = make_vec(2, d);
    L len  = eval(cons(atom("length"),  cons(v, nil)), env);
    L len2 = eval(cons(atom("length2"), cons(v, nil)), env);
    r2_assert("length [3 4] == 5",   equ(len,  (L)5.0));
    r2_assert("length2 [3 4] == 25", equ(len2, (L)25.0));
    return NULL;
}

static const char *test_tensor_dist(void)
{
    setup();
    float a[] = {0.f, 0.f};
    float b[] = {3.f, 4.f};
    L va = make_vec(2, a), vb = make_vec(2, b);
    L d  = eval(cons(atom("dist"),  cons(va, cons(vb, nil))), env);
    L d2 = eval(cons(atom("dist2"), cons(va, cons(vb, nil))), env);
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
    L eq  = eval(cons(atom("vec="), cons(va, cons(vb, nil))), env);
    L neq = eval(cons(atom("vec="), cons(va, cons(vc, nil))), env);
    r2_assert("vec= equal tensors is #t",     equ(eq, tru));
    r2_assert("vec= unequal tensors is nil",  equ(neq, nil));
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

    L h = eval(cons(atom("head"), cons(v, nil)), env);
    r2_assert("head of vec == 10", equ(h, (L)10.0));

    L tl = eval(cons(atom("tail"), cons(v, nil)), env);
    r2_assert("tail is TENS",        T(tl) == TENS);
    r2_assert("tail len == 2",       tensor_heap[ord(tl)].len == 2);
    r2_assert("tail[0] == 20",       tensor_heap[ord(tl)].data[0] == 20.f);
    r2_assert("tail[1] == 30",       tensor_heap[ord(tl)].data[1] == 30.f);
    return NULL;
}

static const char *test_tensor_head_tail_mat(void)
{
    setup();
    float d[] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
    L m = make_mat(2, 3, d);

    /* head of 2x3 => first row [1 2 3] */
    L h = eval(cons(atom("head"), cons(m, nil)), env);
    r2_assert("head of mat is TENS",     T(h) == TENS);
    r2_assert("head row rank == 1",      tensor_heap[ord(h)].rank == 1);
    r2_assert("head row[0] == 1",        tensor_heap[ord(h)].data[0] == 1.f);
    r2_assert("head row[2] == 3",        tensor_heap[ord(h)].data[2] == 3.f);

    /* tail of 2x3 => [[4 5 6]] (1x3) */
    L tl = eval(cons(atom("tail"), cons(m, nil)), env);
    r2_assert("tail of mat is TENS",     T(tl) == TENS);
    r2_assert("tail shape[0] == 1",      tensor_heap[ord(tl)].shape[0] == 1);
    r2_assert("tail shape[1] == 3",      tensor_heap[ord(tl)].shape[1] == 3);
    r2_assert("tail[0][0] == 4",         tensor_heap[ord(tl)].data[0] == 4.f);
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

    r2_assert("dot vec2",     equ(eval(cons(atom("dot"),     cons(va, cons(va, nil))), env), (L)25.0));
    r2_assert("length vec2",  equ(eval(cons(atom("length"),  cons(va, nil)),           env), (L)5.0));
    r2_assert("length2 vec2", equ(eval(cons(atom("length2"), cons(va, nil)),           env), (L)25.0));
    r2_assert("dist vec2",    equ(eval(cons(atom("dist"),    cons(vb, cons(va, nil))), env), (L)5.0));
    r2_assert("dist2 vec2",   equ(eval(cons(atom("dist2"),   cons(vb, cons(va, nil))), env), (L)25.0));

    L n = eval(cons(atom("normalize"), cons(va, nil)), env);
    r2_assert("normalize vec2[0] == 0.6", tensor_heap[ord(n)].data[0] == 0.6f);
    r2_assert("normalize vec2[1] == 0.8", tensor_heap[ord(n)].data[1] == 0.8f);
    return NULL;
}

static const char *test_tensor_fastpath_vec4(void)
{
    setup();
    float a[] = {1.f, 0.f, 0.f, 0.f};
    float b[] = {0.f, 1.f, 0.f, 0.f};
    L va = make_vec(4, a), vb = make_vec(4, b);

    r2_assert("dot vec4 orthogonal == 0", equ(eval(cons(atom("dot"), cons(va, cons(vb, nil))), env), (L)0.0));
    r2_assert("length vec4 unit == 1",    equ(eval(cons(atom("length"), cons(va, nil)), env), (L)1.0));

    float c[] = {-1.f, 4.f, -9.f, 16.f};
    L vc = make_vec(4, c);
    L ab = eval(cons(atom("abs"),  cons(vc, nil)), env);
    L sq = eval(cons(atom("sqrt"), cons(vc, nil)), env);  /* sqrt of abs vals */
    r2_assert("abs vec4[0] == 1",  tensor_heap[ord(ab)].data[0] == 1.f);
    r2_assert("abs vec4[2] == 9",  tensor_heap[ord(ab)].data[2] == 9.f);

    float pos[] = {4.f, 9.f, 16.f, 25.f};
    L vp = make_vec(4, pos);
    L sr = eval(cons(atom("sqrt"), cons(vp, nil)), env);
    r2_assert("sqrt vec4[0] == 2", tensor_heap[ord(sr)].data[0] == 2.f);
    r2_assert("sqrt vec4[3] == 5", tensor_heap[ord(sr)].data[3] == 5.f);
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
    L def1 = cons(atom("define"),
                  cons(atom("\xE4\xBB\x96"), cons((L)3.0, nil))); /* 他 */
    eval(def1, env);
    r2_assert("CJK atom as value",
              equ(eval(atom("\xE4\xBB\x96"), env), (L)3.0));

    /* use in arithmetic */
    L add = cons(atom("+"),
                 cons(atom("\xE4\xBB\x96"), cons((L)1.0, nil)));
    r2_assert("CJK atom in arithmetic",
              equ(eval(add, env), (L)4.0));

    /* store under emoji, use in expression */
    L def2 = cons(atom("define"),
                  cons(atom("\xF0\x9F\x94\xA5"), cons((L)100.0, nil))); /* 🔥 */
    eval(def2, env);
    L mul = cons(atom("*"),
                 cons(atom("\xF0\x9F\x94\xA5"), cons((L)2.0, nil)));
    r2_assert("emoji atom in arithmetic",
              equ(eval(mul, env), (L)200.0));

    /* store a tensor under a multi-byte name */
    float d[] = {1.f, 2.f, 3.f};
    L v = make_vec(3, d);
    L def3 = cons(atom("define"), cons(atom("\xCF\x80\xCF\x80"), cons(v, nil))); /* ππ */
    eval(def3, env);
    L sh = eval(cons(atom("shape"), cons(atom("\xCF\x80\xCF\x80"), nil)), env);
    r2_assert("multi-byte atom stores tensor",
              tensor_heap[ord(sh)].data[0] == 3.f);

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
   make-tensor primitive
   --------------------------------------------------------------------- */

static const char *test_make_tensor_scalars(void)
{
    setup();
    /* (make-tensor 1 2 3) - three scalars become a rank-1 vector */
    L expr = cons(atom("make-tensor"), cons((L)1.0, cons((L)2.0, cons((L)3.0, nil))));
    L r = eval(expr, env);
    r2_assert("scalars: is TENS",    T(r) == TENS);
    r2_assert("scalars: rank == 1",  tensor_heap[ord(r)].rank == 1);
    r2_assert("scalars: len == 3",   tensor_heap[ord(r)].len == 3);
    r2_assert("scalars: data[0]==1", tensor_heap[ord(r)].data[0] == 1.f);
    r2_assert("scalars: data[1]==2", tensor_heap[ord(r)].data[1] == 2.f);
    r2_assert("scalars: data[2]==3", tensor_heap[ord(r)].data[2] == 3.f);
    return NULL;
}

static const char *test_make_tensor_expr(void)
{
    setup();
    /* (make-tensor (+ 1 2) 4) - s-expression as a tensor element */
    L add  = cons(atom("+"), cons((L)1.0, cons((L)2.0, nil)));
    L expr = cons(atom("make-tensor"), cons(add, cons((L)4.0, nil)));
    L r = eval(expr, env);
    r2_assert("expr: is TENS",    T(r) == TENS);
    r2_assert("expr: data[0]==3", tensor_heap[ord(r)].data[0] == 3.f);
    r2_assert("expr: data[1]==4", tensor_heap[ord(r)].data[1] == 4.f);
    return NULL;
}

static const char *test_make_tensor_var(void)
{
    setup();
    /* (define x 5) then (make-tensor x 2 3) - variable lookup inside tensor */
    L def  = cons(atom("define"), cons(atom("x"), cons((L)5.0, nil)));
    eval(def, env);
    L expr = cons(atom("make-tensor"), cons(atom("x"), cons((L)2.0, cons((L)3.0, nil))));
    L r = eval(expr, env);
    r2_assert("var: is TENS",    T(r) == TENS);
    r2_assert("var: data[0]==5", tensor_heap[ord(r)].data[0] == 5.f);
    r2_assert("var: data[1]==2", tensor_heap[ord(r)].data[1] == 2.f);
    r2_assert("var: data[2]==3", tensor_heap[ord(r)].data[2] == 3.f);
    return NULL;
}

static const char *test_make_tensor_stack(void)
{
    setup();
    /* (make-tensor (make-tensor 1 2) (make-tensor 3 4)) - stacks rows into [[1 2][3 4]] */
    L row0 = cons(atom("make-tensor"), cons((L)1.0, cons((L)2.0, nil)));
    L row1 = cons(atom("make-tensor"), cons((L)3.0, cons((L)4.0, nil)));
    L expr = cons(atom("make-tensor"), cons(row0, cons(row1, nil)));
    L r = eval(expr, env);
    r2_assert("stack: is TENS",       T(r) == TENS);
    r2_assert("stack: rank == 2",     tensor_heap[ord(r)].rank == 2);
    r2_assert("stack: shape[0] == 2", tensor_heap[ord(r)].shape[0] == 2);
    r2_assert("stack: shape[1] == 2", tensor_heap[ord(r)].shape[1] == 2);
    r2_assert("stack: data[0] == 1",  tensor_heap[ord(r)].data[0] == 1.f);
    r2_assert("stack: data[3] == 4",  tensor_heap[ord(r)].data[3] == 4.f);
    return NULL;
}

static const char *test_make_tensor_expr_in_matrix(void)
{
    setup();
    /* The motivating use case: define x=3, build [(+ 3 x) x] which should give [6 3] */
    L def  = cons(atom("define"), cons(atom("x"), cons((L)3.0, nil)));
    eval(def, env);
    L add  = cons(atom("+"), cons((L)3.0, cons(atom("x"), nil)));
    L expr = cons(atom("make-tensor"), cons(add, cons(atom("x"), nil)));
    L r = eval(expr, env);
    r2_assert("mat-expr: is TENS",    T(r) == TENS);
    r2_assert("mat-expr: data[0]==6", tensor_heap[ord(r)].data[0] == 6.f);
    r2_assert("mat-expr: data[1]==3", tensor_heap[ord(r)].data[1] == 3.f);
    return NULL;
}

/* -----------------------------------------------------------------------
   homoiconicity -- tensor literals as code-as-data
   --------------------------------------------------------------------- */

static const char *test_eval_quoted_tensor(void)
{
    setup();
    /* (eval '[1 2 3]) - quote prevents evaluation, eval triggers it */
    L mt   = cons(atom("make-tensor"), cons((L)1.0, cons((L)2.0, cons((L)3.0, nil))));
    L expr = cons(atom("eval"), cons(cons(atom("quote"), cons(mt, nil)), nil));
    L r = eval(expr, env);
    r2_assert("quoted eval: is TENS",    T(r) == TENS);
    r2_assert("quoted eval: data[0]==1", tensor_heap[ord(r)].data[0] == 1.f);
    r2_assert("quoted eval: data[2]==3", tensor_heap[ord(r)].data[2] == 3.f);
    return NULL;
}

static const char *test_define_code_eval_later(void)
{
    setup();
    /* (define code '[(+ 3 x) x]) then (define x 4) then (eval code)
       the expression is stored unevaluated and uses whatever x is at eval time */
    L mt     = cons(atom("make-tensor"),
                    cons(cons(atom("+"), cons((L)3.0, cons(atom("x"), nil))),
                         cons(atom("x"), nil)));
    L def_code = cons(atom("define"),
                      cons(atom("code"), cons(cons(atom("quote"), cons(mt, nil)), nil)));
    eval(def_code, env);
    L def_x = cons(atom("define"), cons(atom("x"), cons((L)4.0, nil)));
    eval(def_x, env);
    L expr = cons(atom("eval"), cons(atom("code"), nil));
    L r = eval(expr, env);
    r2_assert("deferred: is TENS",    T(r) == TENS);
    r2_assert("deferred: data[0]==7", tensor_heap[ord(r)].data[0] == 7.f);
    r2_assert("deferred: data[1]==4", tensor_heap[ord(r)].data[1] == 4.f);
    return NULL;
}

static const char *test_lambda_tensor_body(void)
{
    setup();
    /* (define make-row (lambda (a b) [a b])) then (make-row 5 6) => [5 6] */
    L body = cons(atom("make-tensor"), cons(atom("a"), cons(atom("b"), nil)));
    L lam  = cons(atom("lambda"),
                  cons(cons(atom("a"), cons(atom("b"), nil)),
                       cons(body, nil)));
    L def  = cons(atom("define"), cons(atom("make-row"), cons(lam, nil)));
    eval(def, env);
    L call = cons(atom("make-row"), cons((L)5.0, cons((L)6.0, nil)));
    L r = eval(call, env);
    r2_assert("lambda body: is TENS",    T(r) == TENS);
    r2_assert("lambda body: data[0]==5", tensor_heap[ord(r)].data[0] == 5.f);
    r2_assert("lambda body: data[1]==6", tensor_heap[ord(r)].data[1] == 6.f);
    return NULL;
}

/* -----------------------------------------------------------------------
   print primitive
   --------------------------------------------------------------------- */

static const char *test_print_returns_value(void)
{
    setup();
    /* (print 42) should return 42 */
    L expr = cons(atom("print"), cons((L)42.0, nil));
    L r = eval(expr, env);
    r2_assert("print: returns numeric value", r == (L)42.0);
    return NULL;
}

static const char *test_print_returns_tensor(void)
{
    setup();
    /* (define v [1 2 3]) (print v) should return the tensor */
    L mk   = cons(atom("make-tensor"),
                  cons((L)1.0, cons((L)2.0, cons((L)3.0, nil))));
    L def  = cons(atom("define"), cons(atom("v"), cons(mk, nil)));
    eval(def, env);
    L expr = cons(atom("print"), cons(atom("v"), nil));
    L r = eval(expr, env);
    r2_assert("print tensor: is TENS",    T(r) == TENS);
    r2_assert("print tensor: data[0]==1", tensor_heap[ord(r)].data[0] == 1.f);
    return NULL;
}

/* -----------------------------------------------------------------------
   set! — in-place mutation
   --------------------------------------------------------------------- */

static const char *test_set_bang_scalar(void)
{
    setup();
    /* (define x 1) (set! x 42) => x is now 42 */
    L def = cons(atom("define"), cons(atom("x"), cons((L)1.0, nil)));
    eval(def, env);
    L set = cons(atom("set!"), cons(atom("x"), cons((L)42.0, nil)));
    L ret = eval(set, env);
    r2_assert("set! returns new value",        equ(ret, (L)42.0));
    r2_assert("set! mutates binding",          equ(eval(atom("x"), env), (L)42.0));
    return NULL;
}

static const char *test_set_bang_tensor(void)
{
    setup();
    /* (define W [1 2 3]) (set! W [7 8 9]) => W is now [7 8 9] */
    float d1[] = {1.f, 2.f, 3.f};
    float d2[] = {7.f, 8.f, 9.f};
    L v1 = make_vec(3, d1);
    L v2 = make_vec(3, d2);
    L def = cons(atom("define"), cons(atom("W"), cons(v1, nil)));
    eval(def, env);
    L set = cons(atom("set!"), cons(atom("W"), cons(v2, nil)));
    eval(set, env);
    L val = eval(atom("W"), env);
    r2_assert("set! tensor: is TENS",     T(val) == TENS);
    r2_assert("set! tensor: data[0]==7",  tensor_heap[ord(val)].data[0] == 7.f);
    r2_assert("set! tensor: data[2]==9",  tensor_heap[ord(val)].data[2] == 9.f);
    return NULL;
}

static const char *test_set_bang_unbound_is_err(void)
{
    setup();
    /* (set! undefined-var 1) => ERR (not defined) */
    L set = cons(atom("set!"), cons(atom("no-such-var"), cons((L)1.0, nil)));
    r2_assert("set! unbound returns ERR", equ(eval(set, env), err));
    return NULL;
}

static const char *test_set_bang_no_shadow(void)
{
    setup();
    /* define once, set! twice — env must not grow with shadow bindings */
    L def = cons(atom("define"), cons(atom("n"), cons((L)0.0, nil)));
    eval(def, env);
    L env_after_define = env;
    L set1 = cons(atom("set!"), cons(atom("n"), cons((L)1.0, nil)));
    eval(set1, env);
    L set2 = cons(atom("set!"), cons(atom("n"), cons((L)2.0, nil)));
    eval(set2, env);
    r2_assert("set! does not grow env",  equ(env, env_after_define));
    r2_assert("n is 2 after two set!s",  equ(eval(atom("n"), env), (L)2.0));
    return NULL;
}

/* -----------------------------------------------------------------------
   rank — scalars and rank-0 tensors
   --------------------------------------------------------------------- */

static const char *test_rank_scalar_number(void)
{
    setup();
    /* (rank 4) => 0  (a plain number is a rank-0 scalar) */
    L expr = cons(atom("rank"), cons((L)4.0, nil));
    r2_assert("rank of plain number is 0", equ(eval(expr, env), (L)0.0));
    return NULL;
}

static const char *test_rank1_single_tensor(void)
{
    setup();
    /* (make-tensor 5) => rank-1 vector with one element, shape [1] */
    L expr = cons(atom("make-tensor"), cons((L)5.0, nil));
    L r = eval(expr, env);
    r2_assert("single-elem tensor is TENS",       T(r) == TENS);
    r2_assert("single-elem tensor rank == 1",     tensor_heap[ord(r)].rank == 1);
    r2_assert("single-elem tensor shape[0] == 1", tensor_heap[ord(r)].shape[0] == 1);
    r2_assert("single-elem tensor data[0]==5",    tensor_heap[ord(r)].data[0] == 5.f);
    /* (rank (make-tensor 5)) => 1 */
    L rank_expr = cons(atom("rank"), cons(expr, nil));
    r2_assert("(rank [5]) == 1", equ(eval(rank_expr, env), (L)1.0));
    return NULL;
}

static const char *test_str_tag_reserved(void)
{
    setup();
    r2_assert("STR tag is 0x7ffe", STR == 0x7ffe);
    r2_assert("STR tag distinct from TENS", STR != TENS);
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
    r2_run_test(test_set_bang_unbound_is_err);
    r2_run_test(test_set_bang_no_shadow);
    r2_run_test(test_rank_scalar_number);
    r2_run_test(test_rank1_single_tensor);
    r2_run_test(test_str_tag_reserved);
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

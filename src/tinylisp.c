#define R2_STRINGS_IMPLEMENTATION
#include "r2_strings.h"
#define R2_MATHS_IMPLEMENTATION
#include "r2_maths.h"
#include "tinylisp.h"

/* hp: top of the atom heap pointer, A+hp with hp=0 points to the first atom string in cell[]
   sp: cell stack pointer, the stack starts at the top of cell[] with sp=N
   safety invariant: hp <= sp<<3 */
I hp = 0, sp = N;

/* atom, primitive, cons, closure, nil, and tensor tags for NaN boxing */
const I ATOM = 0x7ff8, PRIM = 0x7ff9, CONS = 0x7ffa, CLOS = 0x7ffb, NIL = 0x7ffc, TENS = 0x7ffd;

/* tensor heap: pool of tensor_t structs; th is the next free slot.
   tensor data arrays are malloc'd and currently never freed (TODO: mark-sweep gc) */
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
    *(uint64_t *)&x = (uint64_t)t << 48 | i;
    return x;
}

I ord(L x)
{
    return (I)(*(uint64_t *)&x); /* narrowed to 32 bits, removing the tag */
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
   (+ n1 n2 ... nk)    sum of n1 to nk
   (- n1 n2 ... nk)    n1 minus sum of n2 to nk
   (* n1 n2 ... nk)    product of n1 to nk
   (/ n1 n2 ... nk)    n1 divided by the product of n2 to nk
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

/* helper: apply a scalar op element-wise, or tensor+tensor, returning new tensor */
static L tens_binop(L a, L b, char op)
{
    I i;
    if (T(a) == TENS)
    {
        tensor_t *ta = &tensor_heap[ord(a)];
        tensor_t *out = alloc_tensor(ta->rank, ta->shape, ta->len, NULL);
        if (T(b) == TENS)
        {
            tensor_t *tb = &tensor_heap[ord(b)];
            for (i = 0; i < ta->len; i++)
            {
                switch (op)
                {
                case '+':
                    out->data[i] = ta->data[i] + tb->data[i];
                    break;
                case '-':
                    out->data[i] = ta->data[i] - tb->data[i];
                    break;
                case '*':
                    out->data[i] = ta->data[i] * tb->data[i];
                    break;
                case '/':
                    out->data[i] = ta->data[i] / tb->data[i];
                    break;
                }
            }
        }
        else
        {
            float s = (float)b;
            for (i = 0; i < ta->len; i++)
            {
                switch (op)
                {
                case '+':
                    out->data[i] = ta->data[i] + s;
                    break;
                case '-':
                    out->data[i] = ta->data[i] - s;
                    break;
                case '*':
                    out->data[i] = ta->data[i] * s;
                    break;
                case '/':
                    out->data[i] = ta->data[i] / s;
                    break;
                }
            }
        }
        return box(TENS, (I)(out - tensor_heap));
    }
    /* scalar op tensor: broadcast scalar on left */
    if (T(b) == TENS)
    {
        tensor_t *tb = &tensor_heap[ord(b)];
        tensor_t *out = alloc_tensor(tb->rank, tb->shape, tb->len, NULL);
        float s = (float)a;
        for (i = 0; i < tb->len; i++)
        {
            switch (op)
            {
            case '+':
                out->data[i] = s + tb->data[i];
                break;
            case '-':
                out->data[i] = s - tb->data[i];
                break;
            case '*':
                out->data[i] = s * tb->data[i];
                break;
            case '/':
                out->data[i] = s / tb->data[i];
                break;
            }
        }
        return box(TENS, (I)(out - tensor_heap));
    }
    /* both scalar */
    switch (op)
    {
    case '+':
        return a + b;
    case '-':
        return a - b;
    case '*':
        return a * b;
    case '/':
        return a / b;
    }
    return err;
}

L f_add(L t, L e)
{
    t = evlis(t, e);
    L n = car(t);
    while (!is_nil(t = cdr(t)))
        n = tens_binop(n, car(t), '+');
    return num(n);
}

L f_sub(L t, L e)
{
    t = evlis(t, e);
    L n = car(t);
    while (!is_nil(t = cdr(t)))
        n = tens_binop(n, car(t), '-');
    return num(n);
}

L f_mul(L t, L e)
{
    t = evlis(t, e);
    L n = car(t);
    while (!is_nil(t = cdr(t)))
        n = tens_binop(n, car(t), '*');
    return num(n);
}

L f_div(L t, L e)
{
    t = evlis(t, e);
    L n = car(t);
    while (!is_nil(t = cdr(t)))
        n = tens_binop(n, car(t), '/');
    return num(n);
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

/* allocate a tensor from the pool, copying shape and data */
tensor_t *alloc_tensor(I rank, const I *shape, I len, const float *data)
{
    I i;
    if (th >= MAX_TENSORS)
        abort();
    tensor_t *t = &tensor_heap[th++];
    t->rank = rank;
    t->len = len;
    for (i = 0; i < rank; i++)
        t->shape[i] = shape[i];
    t->data = malloc(len * sizeof(float));
    if (!t->data)
        abort();
    if (data)
        for (i = 0; i < len; i++)
            t->data[i] = data[i];
    else
        for (i = 0; i < len; i++)
            t->data[i] = 0.f;
    return t;
}

/* (shape t) → rank-1 tensor of dimension sizes */
L f_shape(L t, L e)
{
    L x = car(evlis(t, e));
    if (T(x) != TENS)
        return err;
    tensor_t *tens = &tensor_heap[ord(x)];
    float sd[MAX_RANK];
    I i, s[1];
    for (i = 0; i < tens->rank; i++)
        sd[i] = (float)tens->shape[i];
    s[0] = tens->rank;
    return box(TENS, (I)(alloc_tensor(1, s, tens->rank, sd) - tensor_heap));
}

/* (rank t) → scalar */
L f_rank(L t, L e)
{
    L x = car(evlis(t, e));
    return T(x) == TENS ? (L)tensor_heap[ord(x)].rank : err;
}

/* (slice t i) → element (scalar) or sub-tensor (row) at index i along axis 0 */
L f_slice(L t, L e)
{
    I i;
    t = evlis(t, e);
    L x = car(t);
    L idx = car(cdr(t));
    if (T(x) != TENS)
        return err;
    tensor_t *tens = &tensor_heap[ord(x)];
    i = (I)idx;
    if (i >= tens->shape[0])
        return err;
    if (tens->rank == 1)
        return (L)tens->data[i];
    /* return a row as a sub-tensor */
    I row = tens->len / tens->shape[0];
    I sh[MAX_RANK];
    I r;
    for (r = 0; r < tens->rank - 1; r++)
        sh[r] = tens->shape[r + 1];
    return box(TENS, (I)(alloc_tensor(tens->rank - 1, sh, row, tens->data + i * row) - tensor_heap));
}

/* (head t) → first element or row (sugar for slice 0) */
L f_head(L t, L e)
{
    L x = car(evlis(t, e));
    if (T(x) != TENS)
        return err;
    tensor_t *tens = &tensor_heap[ord(x)];
    if (tens->rank == 1)
        return (L)tens->data[0];
    I row = tens->len / tens->shape[0];
    I sh[MAX_RANK];
    I i;
    for (i = 0; i < tens->rank - 1; i++)
        sh[i] = tens->shape[i + 1];
    return box(TENS, (I)(alloc_tensor(tens->rank - 1, sh, row, tens->data) - tensor_heap));
}

/* (tail t) → all elements after the first (rank-1: subvector, rank-2: submatrix) */
L f_tail(L t, L e)
{
    L x = car(evlis(t, e));
    if (T(x) != TENS)
        return err;
    tensor_t *tens = &tensor_heap[ord(x)];
    if (tens->shape[0] < 2)
        return err;
    I sh[MAX_RANK];
    I i;
    for (i = 0; i < tens->rank; i++)
        sh[i] = tens->shape[i];
    sh[0] = tens->shape[0] - 1;
    I row = tens->len / tens->shape[0];
    I new_len = sh[0] * row;
    return box(TENS, (I)(alloc_tensor(tens->rank, sh, new_len, tens->data + row) - tensor_heap));
}

/* (tensor? x) → #t if x is a tensor */
L f_tensor_p(L t, L e)
{
    L x = car(evlis(t, e));
    return T(x) == TENS ? tru : nil;
}

/* (matmul A B) / (@ A B) — matrix product.
   Accepts rank-2 * rank-2, rank-2 * rank-1 (mat*col-vec), and
   rank-1 * rank-2 (row-vec*mat).  Result is rank-1 when either
   argument is rank-1, otherwise rank-2. */
L f_matmul(L t, L e)
{
    t = evlis(t, e);
    L xa = car(t);
    L xb = car(cdr(t));
    if (T(xa) != TENS || T(xb) != TENS)
        return err;
    tensor_t *a = &tensor_heap[ord(xa)];
    tensor_t *b = &tensor_heap[ord(xb)];

    /* determine effective dimensions, treating rank-1 as a row (1×n) or
       column (n×1) vector depending on position */
    I r1 = (a->rank == 1) ? 1 : a->shape[0];
    I c1 = (a->rank == 1) ? a->len : a->shape[1];
    I r2 = (b->rank == 2) ? b->shape[0] : b->len;
    I c2 = (b->rank == 2) ? b->shape[1] : 1;
    if (c1 != r2)
        return err;

    I out_len = r1 * c2;
    float *out_data = malloc(out_len * sizeof(float));
    if (!out_data)
        abort();
    mat_mul(a->data, b->data, (unsigned char)r1, (unsigned char)c1, (unsigned char)r2, (unsigned char)c2, out_data);
    L result;
    /* return rank-1 when either input was a vector */
    if (a->rank == 1 || b->rank == 1)
    {
        I len = (a->rank == 1) ? c2 : r1;
        I sh[1];
        sh[0] = len;
        result = box(TENS, (I)(alloc_tensor(1, sh, len, out_data) - tensor_heap));
    }
    else
    {
        I sh[2];
        sh[0] = r1;
        sh[1] = c2;
        result = box(TENS, (I)(alloc_tensor(2, sh, out_len, out_data) - tensor_heap));
    }
    free(out_data);
    return result;
}

/* (transpose M) → rank-2 tensor with rows and columns swapped */
L f_transpose(L t, L e)
{
    L x = car(evlis(t, e));
    if (T(x) != TENS)
        return err;
    tensor_t *a = &tensor_heap[ord(x)];
    if (a->rank != 2)
        return err;
    I r = a->shape[0], c = a->shape[1];
    float *out = malloc(r * c * sizeof(float));
    if (!out)
        abort();
    mat_transpose(a->data, (unsigned char)r, (unsigned char)c, out);
    I sh[2];
    sh[0] = c;
    sh[1] = r;
    L result = box(TENS, (I)(alloc_tensor(2, sh, r * c, out) - tensor_heap));
    free(out);
    return result;
}

/* ---- fast-path dispatch macros for vec2 (len=2) and vec4 (len=4) ----
   vec2 is a union of float[2]; vec4 is a union of float[4].
   Casting our flat float* data to these pointer types is safe for those
   exact sizes.  len=3 uses vecn_* because vec3 is the same 4-float union
   as vec4 — casting a malloc'd float[3] to vec3* would over-read. */

/* dispatch unary tensor→tensor: vec2/vec4 fast path, else vecn_* */
#define TENS_UNARY_DISP(fn2, fn4, fnn)                                                                                 \
    L x = car(evlis(t, e));                                                                                            \
    if (T(x) != TENS)                                                                                                  \
        return err;                                                                                                    \
    tensor_t *a = &tensor_heap[ord(x)];                                                                                \
    tensor_t *out = alloc_tensor(a->rank, a->shape, a->len, NULL);                                                     \
    if (a->len == 2)                                                                                                   \
        fn2((const vec2 *)a->data, (vec2 *)out->data);                                                                 \
    else if (a->len == 4)                                                                                              \
        fn4((const vec4 *)a->data, (vec4 *)out->data);                                                                 \
    else                                                                                                               \
        fnn(a->data, a->len, out->data);                                                                               \
    return box(TENS, (I)(out - tensor_heap));

/* dispatch binary tensor→scalar: vec2/vec4 fast path, else vecn_* */
#define TENS_BINARY_SCALAR_DISP(fn2, fn4, fnn)                                                                         \
    t = evlis(t, e);                                                                                                   \
    L xa = car(t), xb = car(cdr(t));                                                                                   \
    if (T(xa) != TENS || T(xb) != TENS)                                                                                \
        return err;                                                                                                    \
    tensor_t *a = &tensor_heap[ord(xa)];                                                                               \
    tensor_t *b = &tensor_heap[ord(xb)];                                                                               \
    if (a->len == 2)                                                                                                   \
        return (L)fn2((const vec2 *)a->data, (const vec2 *)b->data);                                                   \
    else if (a->len == 4)                                                                                              \
        return (L)fn4((const vec4 *)a->data, (const vec4 *)b->data);                                                   \
    else                                                                                                               \
        return (L)fnn(a->data, b->data, (int)a->len);

/* dispatch unary tensor→scalar: vec2/vec4 fast path, else vecn_* */
#define TENS_UNARY_SCALAR_DISP(fn2, fn4, fnn)                                                                          \
    L x = car(evlis(t, e));                                                                                            \
    if (T(x) != TENS)                                                                                                  \
        return err;                                                                                                    \
    tensor_t *a = &tensor_heap[ord(x)];                                                                                \
    if (a->len == 2)                                                                                                   \
        return (L)fn2((const vec2 *)a->data);                                                                          \
    else if (a->len == 4)                                                                                              \
        return (L)fn4((const vec4 *)a->data);                                                                          \
    else                                                                                                               \
        return (L)fnn(a->data, (int)a->len);

/* (abs v) — element-wise absolute value; vec4 fast path */
L f_vabs(L t, L e)
{
    L x = car(evlis(t, e));
    if (T(x) != TENS)
        return err;
    tensor_t *a = &tensor_heap[ord(x)];
    tensor_t *out = alloc_tensor(a->rank, a->shape, a->len, NULL);
    if (a->len == 4)
        vec4_abs((const vec4 *)a->data, (vec4 *)out->data);
    else
        vecn_abs(a->data, a->len, out->data);
    return box(TENS, (I)(out - tensor_heap));
}

/* (sqrt v) — element-wise square root; vec4 fast path */
L f_vsqrt(L t, L e)
{
    L x = car(evlis(t, e));
    if (T(x) != TENS)
        return err;
    tensor_t *a = &tensor_heap[ord(x)];
    tensor_t *out = alloc_tensor(a->rank, a->shape, a->len, NULL);
    if (a->len == 4)
        vec4_sqrt((const vec4 *)a->data, (vec4 *)out->data);
    else
        vecn_sqrt(a->data, a->len, out->data);
    return box(TENS, (I)(out - tensor_heap));
}

/* (normalize v) — scale to unit length; vec2/vec4 fast paths */
L f_normalize(L t, L e){TENS_UNARY_DISP(vec2_normalize, vec4_normalize, vecn_normalize)}

/* (pow v exp) — element-wise v^exp; vec2/vec4 fast paths */
L f_vpow(L t, L e)
{
    t = evlis(t, e);
    L x = car(t);
    L xp = car(cdr(t));
    if (T(x) != TENS)
        return err;
    tensor_t *a = &tensor_heap[ord(x)];
    tensor_t *out = alloc_tensor(a->rank, a->shape, a->len, NULL);
    float ep = (float)xp;
    if (a->len == 2)
        vec2_pow((const vec2 *)a->data, ep, (vec2 *)out->data);
    else if (a->len == 4)
        vec4_pow((const vec4 *)a->data, ep, (vec4 *)out->data);
    else
        vecn_pow(a->data, ep, a->len, out->data);
    return box(TENS, (I)(out - tensor_heap));
}

/* (zero n) — rank-1 tensor of n zeros */
L f_zero(L t, L e)
{
    L x = car(evlis(t, e));
    I n = (I)x;
    I sh[1];
    sh[0] = n;
    tensor_t *out = alloc_tensor(1, sh, n, NULL);
    vecn_zero(out->data, (int)n);
    return box(TENS, (I)(out - tensor_heap));
}

/* (dot v1 v2) — dot product → scalar; vec2/vec4 fast paths */
L f_dot(L t, L e){TENS_BINARY_SCALAR_DISP(vec2_dot, vec4_dot, vecn_dot)}

/* (length v) — Euclidean length → scalar; vec2/vec4 fast paths */
L f_length(L t, L e){TENS_UNARY_SCALAR_DISP(vec2_length, vec4_length, vecn_length)}

/* (length2 v) — length squared → scalar; vec2/vec4 fast paths */
L f_length2(L t, L e){TENS_UNARY_SCALAR_DISP(vec2_length_sqrd, vec4_length_sqrd, vecn_length_sqrd)}

/* (dist v1 v2) — Euclidean distance → scalar; vec2/vec4 fast paths */
L f_dist(L t, L e){TENS_BINARY_SCALAR_DISP(vec2_dist, vec4_dist, vecn_dist)}

/* (dist2 v1 v2) — distance squared → scalar; vec2/vec4 fast paths */
L f_dist2(L t, L e){TENS_BINARY_SCALAR_DISP(vec2_dist_sqrd, vec4_dist_sqrd, vecn_dist_sqrd)}

/* (vec= v1 v2) — element-wise equality → #t or () */
L f_veq(L t, L e)
{
    t = evlis(t, e);
    L xa = car(t), xb = car(cdr(t));
    if (T(xa) != TENS || T(xb) != TENS)
        return err;
    tensor_t *a = &tensor_heap[ord(xa)];
    tensor_t *b = &tensor_heap[ord(xb)];
    return vecn_equals(a->data, b->data, (int)a->len) ? tru : nil;
}

/* table of Lisp primitives, each has a name s and function pointer f */
struct prims prim[MAX_PRIMS] = {{"eval", f_eval},
                                {"quote", f_quote},
                                {"cons", f_cons},
                                {"car", f_car},
                                {"cdr", f_cdr},
                                {"+", f_add},
                                {"-", f_sub},
                                {"*", f_mul},
                                {"/", f_div},
                                {"int", f_int},
                                {"<", f_lt},
                                {"eq?", f_eq},
                                {"pair?", f_pair},
                                {"or", f_or},
                                {"and", f_and},
                                {"not", f_not},
                                {"cond", f_cond},
                                {"if", f_if},
                                {"let*", f_leta},
                                {"lambda", f_lambda},
                                {"define", f_define},
                                {"shape", f_shape},
                                {"rank", f_rank},
                                {"slice", f_slice},
                                {"head", f_head},
                                {"tail", f_tail},
                                {"tensor?", f_tensor_p},
                                {"matmul", f_matmul},
                                {"@", f_matmul},
                                {"transpose", f_transpose},
                                {"T", f_transpose},
                                {"abs", f_vabs},
                                {"sqrt", f_vsqrt},
                                {"normalize", f_normalize},
                                {"pow", f_vpow},
                                {"zero", f_zero},
                                {"dot", f_dot},
                                {"length", f_length},
                                {"length2", f_length2},
                                {"dist", f_dist},
                                {"dist2", f_dist2},
                                {"vec=", f_veq},
                                {0}};
int prim_count = 41;

void register_prim(const char *s, L (*f)(L, L))
{
    prim[prim_count].s = s;
    prim[prim_count].f = f;
    prim_count++;
    // keep the sentinel intact
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

/* return nonzero if we are looking at character c, ' ' means any white space.
   commas are treated as whitespace so [1, 2, 3] == [1 2 3] */
I seeing(int c)
{
    return c == ' ' ? (see > 0 && see <= c) || see == ',' : see == c;
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
    while (seeing(' '))
        look();
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

/* parse a tensor literal [ e1 e2 ... ] or [ [r1...] [r2...] ]
   called after [ has already been scanned into buf */
L tensor_lit(void)
{
    float data[1024];
    I count = 0, inner_size = 0, is_matrix = 0, r;
    I shape[2];

    while (scan() != ']')
    {
        if (*buf == '[')
        {
            /* nested row for rank-2 matrix */
            I row_start = count;
            is_matrix = 1;
            while (scan() != ']')
            {
                L x = atomic();
                if (count < 1024)
                    data[count++] = (float)x;
            }
            if (inner_size == 0)
                inner_size = count - row_start;
        }
        else
        {
            L x = atomic();
            if (count < 1024)
                data[count++] = (float)x;
        }
    }

    if (is_matrix && inner_size > 0)
    {
        shape[0] = count / inner_size;
        shape[1] = inner_size;
        r = 2;
    }
    else
    {
        shape[0] = count;
        r = 1;
    }
    return box(TENS, (I)(alloc_tensor(r, shape, count, data) - tensor_heap));
}

/* return a parsed Lisp expression */
L parse(void)
{
    return *buf == '(' ? list() : *buf == '[' ? tensor_lit() : *buf == '\'' ? quote() : atomic();
}

/* display a Lisp list t */
void printlist(L t)
{
    for (putchar('(');; putchar(' '))
    {
        print(car(t));
        t = cdr(t);
        if (is_nil(t))
            break;
        if (T(t) != CONS)
        {
            printf(" . ");
            print(t);
            break;
        }
    }
    putchar(')');
}

/* display a Lisp expression x */
/* recursively print a sub-tensor rooted at data[offset] with given shape/rank */
static void print_tensor(const float *data, const I *shape, I rank, I offset)
{
    if (data == NULL)
    {
        return;
    }

    I i;
    printf("[");
    if (rank == 1)
    {
        for (i = 0; i < shape[0]; i++)
        {
            if (i > 0)
                printf(" ");
            printf("%.6g", data[offset + i]);
        }
    }
    else
    {
        /* stride = product of remaining dimensions */
        I stride = 1;
        for (i = 1; i < rank; i++)
            stride *= shape[i];
        for (i = 0; i < shape[0]; i++)
        {
            if (i > 0)
                printf(" ");
            print_tensor(data, shape + 1, rank - 1, offset + i * stride);
        }
    }
    printf("]");
}

void print(L x)
{
    if (T(x) == NIL)
        printf("()");
    else if (T(x) == ATOM)
        printf("%s", A + ord(x));
    else if (T(x) == PRIM)
        printf("<%s>", prim[ord(x)].s);
    else if (T(x) == CONS)
        printlist(x);
    else if (T(x) == CLOS)
        printf("{%u}", ord(x));
    else if (T(x) == TENS)
    {
        tensor_t *t = &tensor_heap[ord(x)];
        print_tensor(t->data, t->shape, t->rank, 0);
    }
    else
        printf("%.10lg", x);
}

/* garbage collection removes temporary cells, keeps global environment */
void gc(void)
{
    I i;
    /* --- cell GC: discard everything above the environment --- */
    sp = ord(env);

    /* --- tensor GC: mark / compact / patch --- */
    if (th == 0)
        return;

    /* 1. mark: scan all live cells for TENS references */
    unsigned char mark[MAX_TENSORS];
    memset(mark, 0, th);
    for (i = sp; i < N; i++)
    {
        L v = cell[i];
        if (T(v) == TENS && ord(v) < th)
            mark[ord(v)] = 1;
    }

    /* 2. compact live tensors to the front; build a remap table */
    I remap[MAX_TENSORS];
    I new_th = 0;
    for (i = 0; i < th; i++)
    {
        if (mark[i])
        {
            remap[i] = new_th;
            if (i != new_th)
                tensor_heap[new_th] = tensor_heap[i];
            new_th++;
        }
        else
        {
            free(tensor_heap[i].data);
            tensor_heap[i].data = NULL;
        }
    }

    /* 3. patch TENS ordinals in all live cells to new positions */
    for (i = sp; i < N; i++)
    {
        L v = cell[i];
        if (T(v) == TENS && ord(v) < th)
            cell[i] = box(TENS, remap[ord(v)]);
    }

    th = new_th;
}

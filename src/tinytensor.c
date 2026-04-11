#define R2_MATHS_IMPLEMENTATION
#include "r2_maths.h"
#include "tinylisp.h"
#include "tinytensor.h"

#include <stdlib.h>
#include <string.h>

/* allocate a tensor from the pool, copying shape and data */
tensor_t *alloc_tensor(lisp_state_t *s, II rank, const II *shape, II len, const float *data)
{
    II i;
    if (s->th >= MAX_TENSORS)
        abort();
    tensor_t *t = &s->tensor_heap[s->th++];
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

/* helper: apply a scalar op element-wise, or tensor+tensor, returning new tensor */
L tens_binop(lisp_state_t *s, L a, L b, char op)
{
    II i;
    if (T(a) == TENS)
    {
        tensor_t *ta = &s->tensor_heap[ord(a)];
        tensor_t *out = alloc_tensor(s, ta->rank, ta->shape, ta->len, NULL);
        if (T(b) == TENS)
        {
            tensor_t *tb = &s->tensor_heap[ord(b)];
            /* row broadcast: (rows x cols) OP (cols,) — add/scale a bias vector
               across every row.  Handles e.g. (@ x W) + bias. */
            if (ta->rank == 2 && tb->rank == 1 && ta->shape[1] == tb->len) {
                II cols = ta->shape[1];
                for (i = 0; i < ta->len; i++) {
                    switch (op) {
                    case '+': out->data[i] = ta->data[i] + tb->data[i % cols]; break;
                    case '-': out->data[i] = ta->data[i] - tb->data[i % cols]; break;
                    case '*': out->data[i] = ta->data[i] * tb->data[i % cols]; break;
                    case '/': out->data[i] = ta->data[i] / tb->data[i % cols]; break;
                    }
                }
                return box(TENS, (II)(out - s->tensor_heap));
            }
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
            float scalar = (float)b;
            for (i = 0; i < ta->len; i++)
            {
                switch (op)
                {
                case '+':
                    out->data[i] = ta->data[i] + scalar;
                    break;
                case '-':
                    out->data[i] = ta->data[i] - scalar;
                    break;
                case '*':
                    out->data[i] = ta->data[i] * scalar;
                    break;
                case '/':
                    out->data[i] = ta->data[i] / scalar;
                    break;
                }
            }
        }
        return box(TENS, (II)(out - s->tensor_heap));
    }
    /* scalar op tensor: broadcast scalar on left */
    if (T(b) == TENS)
    {
        tensor_t *tb = &s->tensor_heap[ord(b)];
        tensor_t *out = alloc_tensor(s, tb->rank, tb->shape, tb->len, NULL);
        float scalar = (float)a;
        for (i = 0; i < tb->len; i++)
        {
            switch (op)
            {
            case '+':
                out->data[i] = scalar + tb->data[i];
                break;
            case '-':
                out->data[i] = scalar - tb->data[i];
                break;
            case '*':
                out->data[i] = scalar * tb->data[i];
                break;
            case '/':
                out->data[i] = scalar / tb->data[i];
                break;
            }
        }
        return box(TENS, (II)(out - s->tensor_heap));
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
    return s->l_err;
}

/* if x is a stored s-expression (CONS), evaluate it fully before use.
   this lets quoted expressions like q='(+ 1 2) be used directly in
   arithmetic and tensor literals, including deeply nested cases. */
static L resolve(lisp_state_t *s, L x, L e)
{
    return T(x) == CONS ? eval(s, x, e) : x;
}

static L f_add(lisp_state_t *s, L t, L e)
{
    t = evlis(s, t, e);
    L n = resolve(s, car(s, t), e);
    while (!is_nil(s, t = cdr(s, t)))
        n = tens_binop(s, n, resolve(s, car(s, t), e), '+');
    return num(n);
}

static L f_sub(lisp_state_t *s, L t, L e)
{
    t = evlis(s, t, e);
    L n = resolve(s, car(s, t), e);
    while (!is_nil(s, t = cdr(s, t)))
        n = tens_binop(s, n, resolve(s, car(s, t), e), '-');
    return num(n);
}

static L f_mul(lisp_state_t *s, L t, L e)
{
    t = evlis(s, t, e);
    L n = resolve(s, car(s, t), e);
    while (!is_nil(s, t = cdr(s, t)))
        n = tens_binop(s, n, resolve(s, car(s, t), e), '*');
    return num(n);
}

static L f_div(lisp_state_t *s, L t, L e)
{
    t = evlis(s, t, e);
    L n = resolve(s, car(s, t), e);
    while (!is_nil(s, t = cdr(s, t)))
        n = tens_binop(s, n, resolve(s, car(s, t), e), '/');
    return num(n);
}

/* ---- fast-path dispatch macros for vec2 (len=2) and vec4 (len=4) ----
   vec2 is a union of float[2]; vec4 is a union of float[4].
   Casting our flat float* data to these pointer types is safe for those
   exact sizes.  len=3 uses vecn_* because vec3 is the same 4-float union
   as vec4 — casting a malloc'd float[3] to vec3* would over-read. */

/* dispatch unary tensor→tensor: vec2/vec4 fast path, else vecn_* */
#define TENS_UNARY_DISP(fn2, fn4, fnn)                                                                                 \
    L x = car(s, evlis(s, t, e));                                                                                            \
    if (T(x) != TENS)                                                                                                  \
        return s->l_err;                                                                                                    \
    tensor_t *a = &s->tensor_heap[ord(x)];                                                                                \
    tensor_t *out = alloc_tensor(s, a->rank, a->shape, a->len, NULL);                                                     \
    if (a->len == 2)                                                                                                   \
        fn2((const vec2 *)a->data, (vec2 *)out->data);                                                                 \
    else if (a->len == 4)                                                                                              \
        fn4((const vec4 *)a->data, (vec4 *)out->data);                                                                 \
    else                                                                                                               \
        fnn(a->data, a->len, out->data);                                                                               \
    return box(TENS, (II)(out - s->tensor_heap));

/* dispatch binary tensor→scalar: vec2/vec4 fast path, else vecn_* */
#define TENS_BINARY_SCALAR_DISP(fn2, fn4, fnn)                                                                         \
    t = evlis(s, t, e);                                                                                                   \
    L xa = car(s, t), xb = car(s, cdr(s, t));                                                                                   \
    if (T(xa) != TENS || T(xb) != TENS)                                                                                \
        return s->l_err;                                                                                                    \
    tensor_t *a = &s->tensor_heap[ord(xa)];                                                                               \
    tensor_t *b = &s->tensor_heap[ord(xb)];                                                                               \
    if (a->len == 2)                                                                                                   \
        return (L)fn2((const vec2 *)a->data, (const vec2 *)b->data);                                                   \
    else if (a->len == 4)                                                                                              \
        return (L)fn4((const vec4 *)a->data, (const vec4 *)b->data);                                                   \
    else                                                                                                               \
        return (L)fnn(a->data, b->data, (int)a->len);

/* dispatch unary tensor→scalar: vec2/vec4 fast path, else vecn_* */
#define TENS_UNARY_SCALAR_DISP(fn2, fn4, fnn)                                                                          \
    L x = car(s, evlis(s, t, e));                                                                                            \
    if (T(x) != TENS)                                                                                                  \
        return s->l_err;                                                                                                    \
    tensor_t *a = &s->tensor_heap[ord(x)];                                                                                \
    if (a->len == 2)                                                                                                   \
        return (L)fn2((const vec2 *)a->data);                                                                          \
    else if (a->len == 4)                                                                                              \
        return (L)fn4((const vec4 *)a->data);                                                                          \
    else                                                                                                               \
        return (L)fnn(a->data, (int)a->len);

/* (shape t) -> rank-1 tensor of dimension sizes */
static L f_shape(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    if (T(x) != TENS)
        return s->l_err;
    tensor_t *tens = &s->tensor_heap[ord(x)];
    float sd[MAX_RANK];
    II i, sh[1];
    for (i = 0; i < tens->rank; i++)
        sd[i] = (float)tens->shape[i];
    sh[0] = tens->rank;
    return box(TENS, (II)(alloc_tensor(s, 1, sh, tens->rank, sd) - s->tensor_heap));
}

/* (rank t) -> scalar; returns 0 for plain numbers (rank-0 scalars) */
static L f_rank(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    if (T(x) != TENS) return 0.0;
    return (L)s->tensor_heap[ord(x)].rank;
}

/* (slice t i) -> element (scalar) or sub-tensor (row) at index i along axis 0 */
static L f_slice(lisp_state_t *s, L t, L e)
{
    II i;
    t = evlis(s, t, e);
    L x = car(s, t);
    L idx = car(s, cdr(s, t));
    if (T(x) != TENS)
        return s->l_err;
    tensor_t *tens = &s->tensor_heap[ord(x)];
    i = (II)idx;
    if (i >= tens->shape[0])
        return s->l_err;
    if (tens->rank == 1)
        return (L)tens->data[i];
    /* return a row as a sub-tensor */
    II row = tens->len / tens->shape[0];
    II sh[MAX_RANK];
    II r;
    for (r = 0; r < tens->rank - 1; r++)
        sh[r] = tens->shape[r + 1];
    return box(TENS, (II)(alloc_tensor(s, tens->rank - 1, sh, row, tens->data + i * row) - s->tensor_heap));
}

/* (first t) -> first element or row (sugar for slice 0); CL-style name */
static L f_head(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    if (T(x) != TENS)
        return s->l_err;
    tensor_t *tens = &s->tensor_heap[ord(x)];
    if (tens->rank == 1)
        return (L)tens->data[0];
    II row = tens->len / tens->shape[0];
    II sh[MAX_RANK];
    II i;
    for (i = 0; i < tens->rank - 1; i++)
        sh[i] = tens->shape[i + 1];
    return box(TENS, (II)(alloc_tensor(s, tens->rank - 1, sh, row, tens->data) - s->tensor_heap));
}

/* (rest t) -> all elements after the first (rank-1: subvector, rank-2: submatrix); CL-style name */
static L f_tail(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    if (T(x) != TENS)
        return s->l_err;
    tensor_t *tens = &s->tensor_heap[ord(x)];
    if (tens->shape[0] < 2)
        return s->l_err;
    II sh[MAX_RANK];
    II i;
    for (i = 0; i < tens->rank; i++)
        sh[i] = tens->shape[i];
    sh[0] = tens->shape[0] - 1;
    II row = tens->len / tens->shape[0];
    II new_len = sh[0] * row;
    return box(TENS, (II)(alloc_tensor(s, tens->rank, sh, new_len, tens->data + row) - s->tensor_heap));
}

/* (tensorp x) -> #t if x is a tensor; CL predicate naming convention */
static L f_tensor_p(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    return T(x) == TENS ? s->l_tru : s->l_nil;
}


/* (matmul A B) / (@ A B) — matrix product */
static L f_matmul(lisp_state_t *s, L t, L e)
{
    t = evlis(s, t, e);
    L xa = car(s, t);
    L xb = car(s, cdr(s, t));
    if (T(xa) != TENS || T(xb) != TENS)
        return s->l_err;
    tensor_t *a = &s->tensor_heap[ord(xa)];
    tensor_t *b = &s->tensor_heap[ord(xb)];

    II r1 = (a->rank == 1) ? 1 : a->shape[0];
    II c1 = (a->rank == 1) ? a->len : a->shape[1];
    II r2 = (b->rank == 2) ? b->shape[0] : b->len;
    II c2 = (b->rank == 2) ? b->shape[1] : 1;
    if (c1 != r2)
        return s->l_err;

    II out_len = r1 * c2;
    float *out_data = malloc(out_len * sizeof(float));
    if (!out_data)
        abort();

    mat_mul(a->data, b->data,
	    (unsigned int)r1, (unsigned int)c1,
	    (unsigned int)r2, (unsigned int)c2,
	    out_data);

    L result;
    if (a->rank == 1 || b->rank == 1)
    {
        II len = (a->rank == 1) ? c2 : r1;
        II sh[1];
        sh[0] = len;
        result = box(TENS, (II)(alloc_tensor(s, 1, sh, len, out_data) - s->tensor_heap));
    }
    else
    {
        II sh[2];
        sh[0] = r1;
        sh[1] = c2;
        result = box(TENS, (II)(alloc_tensor(s, 2, sh, out_len, out_data) - s->tensor_heap));
    }
    free(out_data);
    return result;
}

/* (transpose M) -> rank-2 tensor with rows and columns swapped */
static L f_transpose(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    if (T(x) != TENS)
        return s->l_err;
    tensor_t *a = &s->tensor_heap[ord(x)];
    if (a->rank != 2)
        return s->l_err;
    II r = a->shape[0], c = a->shape[1];
    float *out = malloc(r * c * sizeof(float));
    if (!out)
        abort();
    mat_transpose(a->data, r, c, out);
    II sh[2];
    sh[0] = c;
    sh[1] = r;
    L result = box(TENS, (II)(alloc_tensor(s, 2, sh, r * c, out) - s->tensor_heap));
    free(out);
    return result;
}

/* (abs v) — absolute value; scalar passthrough, element-wise on tensors */
static L f_vabs(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    if (T(x) != TENS)
        return (double)x < 0.0 ? (L)(-(double)x) : x;
    tensor_t *a = &s->tensor_heap[ord(x)];
    tensor_t *out = alloc_tensor(s, a->rank, a->shape, a->len, NULL);
    if (a->len == 4)
        vec4_abs((const vec4 *)a->data, (vec4 *)out->data);
    else
        vecn_abs(a->data, a->len, out->data);
    return box(TENS, (II)(out - s->tensor_heap));
}

/* (sqrt v) — square root; scalar passthrough, element-wise on tensors */
static L f_vsqrt(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    if (T(x) != TENS)
        return (L)sqrt((double)x);
    tensor_t *a = &s->tensor_heap[ord(x)];
    tensor_t *out = alloc_tensor(s, a->rank, a->shape, a->len, NULL);
    if (a->len == 4)
        vec4_sqrt((const vec4 *)a->data, (vec4 *)out->data);
    else
        vecn_sqrt(a->data, a->len, out->data);
    return box(TENS, (II)(out - s->tensor_heap));
}

/* (exp x) — e^x; works on scalars and element-wise on tensors */
static L f_exp(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    II i;
    if (T(x) != TENS)
        return (L)exp((double)x);
    tensor_t *a = &s->tensor_heap[ord(x)];
    tensor_t *out = alloc_tensor(s, a->rank, a->shape, a->len, NULL);
    for (i = 0; i < a->len; i++)
        out->data[i] = expf(a->data[i]);
    return box(TENS, (II)(out - s->tensor_heap));
}

/* (tanh x) — hyperbolic tangent; scalar or element-wise on tensors.
   Uses the C standard tanhf() which is numerically stable for all inputs. */
static L f_tanh(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    II i;
    if (T(x) != TENS)
        return (L)tanh((double)x);
    tensor_t *a = &s->tensor_heap[ord(x)];
    tensor_t *out = alloc_tensor(s, a->rank, a->shape, a->len, NULL);
    for (i = 0; i < a->len; i++)
        out->data[i] = tanhf(a->data[i]);
    return box(TENS, (II)(out - s->tensor_heap));
}

/* (sin x) — sine; scalar or element-wise on tensors */
static L f_sin(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    II i;
    if (T(x) != TENS)
        return (L)sin((double)x);
    tensor_t *a = &s->tensor_heap[ord(x)];
    tensor_t *out = alloc_tensor(s, a->rank, a->shape, a->len, NULL);
    for (i = 0; i < a->len; i++)
        out->data[i] = sinf(a->data[i]);
    return box(TENS, (II)(out - s->tensor_heap));
}

/* (cos x) — cosine; scalar or element-wise on tensors */
static L f_cos(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    II i;
    if (T(x) != TENS)
        return (L)cos((double)x);
    tensor_t *a = &s->tensor_heap[ord(x)];
    tensor_t *out = alloc_tensor(s, a->rank, a->shape, a->len, NULL);
    for (i = 0; i < a->len; i++)
        out->data[i] = cosf(a->data[i]);
    return box(TENS, (II)(out - s->tensor_heap));
}

/* (normalize v) — scale to unit length; vec2/vec4 fast paths */
static L f_normalize(lisp_state_t *s, L t, L e){TENS_UNARY_DISP(vec2_normalize, vec4_normalize, vecn_normalize)}

/* (pow v exp) — element-wise v^exp; vec2/vec4 fast paths */
static L f_vpow(lisp_state_t *s, L t, L e)
{
    t = evlis(s, t, e);
    L x = car(s, t);
    L xp = car(s, cdr(s, t));
    if (T(x) != TENS)
        return s->l_err;
    tensor_t *a = &s->tensor_heap[ord(x)];
    tensor_t *out = alloc_tensor(s, a->rank, a->shape, a->len, NULL);
    float ep = (float)xp;
    if (a->len == 2)
        vec2_pow((const vec2 *)a->data, ep, (vec2 *)out->data);
    else if (a->len == 4)
        vec4_pow((const vec4 *)a->data, ep, (vec4 *)out->data);
    else
        vecn_pow(a->data, ep, a->len, out->data);
    return box(TENS, (II)(out - s->tensor_heap));
}

/* (zero n) — rank-1 tensor of n zeros */
static L f_zero(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    II n = (II)x;
    II sh[1];
    sh[0] = n;
    tensor_t *out = alloc_tensor(s, 1, sh, n, NULL);
    vecn_zero(out->data, (int)n);
    return box(TENS, (II)(out - s->tensor_heap));
}

/* (causal-mask n) -> (n x n) lower-triangular mask: 0.0 on/below diagonal, -1e9 above.
   Added to scores before softmax so future positions get ~zero weight. */
static L f_causal_mask(lisp_state_t *s, L t, L e)
{
    II i, j;
    t = evlis(s, t, e);
    II n = (II)(double)car(s, t);
    if (n <= 0) return s->l_err;
    II sh[2] = {n, n};
    tensor_t *out = alloc_tensor(s, 2, sh, n * n, NULL);
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            out->data[i * n + j] = (j <= i) ? 0.0f : -1e9f;
    return box(TENS, (II)(out - s->tensor_heap));
}

/* (dot v1 v2) — dot product → scalar; vec2/vec4 fast paths */
static L f_dot(lisp_state_t *s, L t, L e){TENS_BINARY_SCALAR_DISP(vec2_dot, vec4_dot, vecn_dot)}

/* (norm v) — Euclidean norm (length) → scalar; vec2/vec4 fast paths */
static L f_length(lisp_state_t *s, L t, L e){TENS_UNARY_SCALAR_DISP(vec2_length, vec4_length, vecn_length)}

/* (norm2 v) — norm squared (length squared) → scalar; vec2/vec4 fast paths */
static L f_length2(lisp_state_t *s, L t, L e){TENS_UNARY_SCALAR_DISP(vec2_length_sqrd, vec4_length_sqrd, vecn_length_sqrd)}

/* (length lst) — number of elements in a Lisp list, CL convention */
static L f_list_length(lisp_state_t *s, L t, L e)
{
    L lst = car(s, evlis(s, t, e));
    if (T(lst) == NIL) return 0.0;
    if (T(lst) != CONS) return s->l_err;
    II n = 0;
    while (T(lst) == CONS) { n++; lst = cdr(s, lst); }
    return (double)n;
}

/* (dist v1 v2) — Euclidean distance → scalar; vec2/vec4 fast paths */
static L f_dist(lisp_state_t *s, L t, L e){TENS_BINARY_SCALAR_DISP(vec2_dist, vec4_dist, vecn_dist)}

/* (dist2 v1 v2) — distance squared -> scalar; vec2/vec4 fast paths */
static L f_dist2(lisp_state_t *s, L t, L e){TENS_BINARY_SCALAR_DISP(vec2_dist_sqrd, vec4_dist_sqrd, vecn_dist_sqrd)}

/* (sum x) -> sum of all elements as scalar; scalar passthrough */
static L f_sum(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    II i;
    if (T(x) != TENS) return x;
    tensor_t *a = &s->tensor_heap[ord(x)];
    double acc = 0.0;
    for (i = 0; i < a->len; i++) acc += (double)a->data[i];
    return (L)acc;
}

/* (amax x) -> maximum element as scalar; scalar passthrough */
static L f_amax(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    II i;
    if (T(x) != TENS) return x;
    tensor_t *a = &s->tensor_heap[ord(x)];
    float m = a->data[0];
    for (i = 1; i < a->len; i++) if (a->data[i] > m) m = a->data[i];
    return (L)(double)m;
}

/* (log x) -> element-wise natural log; scalar passthrough */
static L f_log(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    II i;
    if (T(x) != TENS) return (L)log((double)x);
    tensor_t *a = &s->tensor_heap[ord(x)];
    tensor_t *out = alloc_tensor(s, a->rank, a->shape, a->len, NULL);
    for (i = 0; i < a->len; i++) out->data[i] = logf(a->data[i]);
    return box(TENS, (II)(out - s->tensor_heap));
}

/* numerically-stable softmax applied in-place to float[n] */
static void softmax_vec(float *v, II n)
{
    II i;
    float m = v[0], s = 0.f;
    for (i = 1; i < n; i++) if (v[i] > m) m = v[i];
    for (i = 0; i < n; i++) { v[i] = expf(v[i] - m); s += v[i]; }
    for (i = 0; i < n; i++) v[i] /= s;
}

/* (softmax x) -> rank-1: vector softmax; rank-2: row-wise softmax */
static L f_softmax(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    II i;
    if (T(x) != TENS) return s->l_err;
    tensor_t *a = &s->tensor_heap[ord(x)];
    tensor_t *out = alloc_tensor(s, a->rank, a->shape, a->len, a->data);
    if (a->rank == 1) {
        softmax_vec(out->data, out->len);
    } else if (a->rank == 2) {
        II cols = a->shape[1];
        for (i = 0; i < a->shape[0]; i++)
            softmax_vec(out->data + i * cols, cols);
    } else {
        return s->l_err;
    }
    return box(TENS, (II)(out - s->tensor_heap));
}

/* layer normalization applied in-place to float[n] with epsilon eps */
static void layernorm_vec(float *v, II n, float eps)
{
    II i;
    double mean = 0.0, var = 0.0, d;
    for (i = 0; i < n; i++) mean += (double)v[i];
    mean /= n;
    for (i = 0; i < n; i++) { d = (double)v[i] - mean; var += d * d; }
    var /= n;
    float scale = 1.f / sqrtf((float)var + eps);
    for (i = 0; i < n; i++) v[i] = (float)((double)v[i] - mean) * scale;
}

/* (layer-norm x eps) -> rank-1: single LN; rank-2: per-row LN (one token per row) */
static L f_layer_norm(lisp_state_t *s, L t, L e)
{
    II i;
    t = evlis(s, t, e);
    L x = car(s, t);
    float eps = (float)(double)car(s, cdr(s, t));
    if (T(x) != TENS) return s->l_err;
    tensor_t *a = &s->tensor_heap[ord(x)];
    tensor_t *out = alloc_tensor(s, a->rank, a->shape, a->len, a->data);
    if (a->rank == 1) {
        layernorm_vec(out->data, out->len, eps);
    } else if (a->rank == 2) {
        II cols = a->shape[1];
        for (i = 0; i < a->shape[0]; i++)
            layernorm_vec(out->data + i * cols, cols, eps);
    } else {
        return s->l_err;
    }
    return box(TENS, (II)(out - s->tensor_heap));
}

/* (reshape x [d0 d1 ...]) -> new tensor with same data but different shape.
   Total element count must match.  Useful for e.g. turning a flat vector into
   a 2-D matrix: (reshape v [1 768]) */
static L f_reshape(lisp_state_t *s, L t, L e)
{
    II i;
    t = evlis(s, t, e);
    L x   = car(s, t);
    L shp = car(s, cdr(s, t));
    if (T(x) != TENS || T(shp) != TENS) return s->l_err;
    tensor_t *src = &s->tensor_heap[ord(x)];
    tensor_t *sv  = &s->tensor_heap[ord(shp)];
    II new_rank = sv->len;
    if (new_rank > MAX_RANK) return s->l_err;
    II new_shape[MAX_RANK];
    II new_len = 1;
    for (i = 0; i < new_rank; i++) {
        new_shape[i] = (II)sv->data[i];
        new_len *= new_shape[i];
    }
    if (new_len != src->len) return s->l_err;
    return box(TENS, (II)(alloc_tensor(s, new_rank, new_shape, src->len, src->data) - s->tensor_heap));
}

/* (slice-range x start end) -> rows (or elements) in [start, end).
   Works on rank-1 (element range) and rank-2 (row range). */
static L f_slice_range(lisp_state_t *s, L t, L e)
{
    II i;
    t = evlis(s, t, e);
    L x     = car(s, t);
    II start = (II)(double)car(s, cdr(s, t));
    II end   = (II)(double)car(s, cdr(s, cdr(s, t)));
    if (T(x) != TENS || start >= end) return s->l_err;
    tensor_t *a = &s->tensor_heap[ord(x)];
    if (end > a->shape[0]) return s->l_err;
    II n = end - start;
    if (a->rank == 1) {
        II sh[1]; sh[0] = n;
        return box(TENS, (II)(alloc_tensor(s, 1, sh, n, a->data + start) - s->tensor_heap));
    }
    II row = a->len / a->shape[0];
    II sh[MAX_RANK];
    sh[0] = n;
    for (i = 1; i < a->rank; i++) sh[i] = a->shape[i];
    return box(TENS, (II)(alloc_tensor(s, a->rank, sh, n * row, a->data + start * row) - s->tensor_heap));
}

/* (col-slice M j) -> extract column j of a rank-2 matrix as a rank-1 vector.
   Avoids allocating the full transposed matrix for single-column lookups
   (e.g. embedding table lookups). */
static L f_col_slice(lisp_state_t *s, L t, L e)
{
    II i;
    t = evlis(s, t, e);
    L x = car(s, t);
    II j = (II)(double)car(s, cdr(s, t));
    if (T(x) != TENS) return s->l_err;
    tensor_t *a = &s->tensor_heap[ord(x)];
    if (a->rank != 2 || j >= a->shape[1]) return s->l_err;
    II rows = a->shape[0], cols = a->shape[1];
    float *buf = malloc(rows * sizeof(float));
    if (!buf) abort();
    for (i = 0; i < rows; i++) buf[i] = a->data[i * cols + j];
    II sh[1]; sh[0] = rows;
    tensor_t *out = alloc_tensor(s, 1, sh, rows, buf);
    free(buf);
    return box(TENS, (II)(out - s->tensor_heap));
}

/* (argmax x) -> index of the maximum element (integer scalar).
   For rank-2, scans all elements and returns the flat index. */
static L f_argmax(lisp_state_t *s, L t, L e)
{
    L x = car(s, evlis(s, t, e));
    II i, best = 0;
    if (T(x) != TENS) return (L)0.0;
    tensor_t *a = &s->tensor_heap[ord(x)];
    for (i = 1; i < a->len; i++)
        if (a->data[i] > a->data[best]) best = i;
    return (L)(double)best;
}

/* shared deep-equality check: same rank, shape, and all elements match */
int tensor_equal(const tensor_t *a, const tensor_t *b)
{
    II i;
    if (a->rank != b->rank || a->len != b->len)
        return 0;
    for (i = 0; i < a->rank; i++)
        if (a->shape[i] != b->shape[i]) return 0;
    return vecn_equals(a->data, b->data, (int)a->len);
}

/* (equalp v1 v2) — element-wise equality -> #t or (); CL equalp does array comparison */
static L f_veq(lisp_state_t *s, L t, L e)
{
    t = evlis(s, t, e);
    L xa = car(s, t), xb = car(s, cdr(s, t));
    if (T(xa) != TENS || T(xb) != TENS)
        return s->l_err;
    return tensor_equal(&s->tensor_heap[ord(xa)], &s->tensor_heap[ord(xb)]) ? s->l_tru : s->l_nil;
}

/* (vstack A B)
   Stack tensor A on top of tensor B row-wise.
   Both must be rank-2 with equal column counts, or rank-1 (treated as 1-row).
   Returns a rank-2 tensor of shape [(rows_A + rows_B) x cols]. */
static L f_vstack(lisp_state_t *s, L t, L e)
{
    II i;
    t = evlis(s, t, e);
    L a = car(s, t), b = car(s, cdr(s, t));
    if (T(a) != TENS || T(b) != TENS) return s->l_err;
    tensor_t *ta = s->tensor_heap + ord(a);
    tensor_t *tb = s->tensor_heap + ord(b);

    II rows_a = ta->rank == 1 ? 1 : ta->shape[0];
    II cols_a = ta->rank == 1 ? ta->len : ta->shape[ta->rank - 1];
    II rows_b = tb->rank == 1 ? 1 : tb->shape[0];
    II cols_b = tb->rank == 1 ? tb->len : tb->shape[tb->rank - 1];

    if (cols_a != cols_b) return s->l_err;

    II rows = rows_a + rows_b;
    II shape[2] = {rows, cols_a};
    tensor_t *out = alloc_tensor(s, 2, shape, rows * cols_a, NULL);
    for (i = 0; i < (II)(rows_a * cols_a); i++)
        out->data[i] = ta->data[i];
    for (i = 0; i < (II)(rows_b * cols_b); i++)
        out->data[rows_a * cols_a + i] = tb->data[i];
    return box(TENS, (II)(out - s->tensor_heap));
}

/* (make-tensor e1 e2 ...) -- runtime backend for [ ] tensor literals */
static L f_make_tensor(lisp_state_t *s, L t, L e)
{
    II n, k, elem_rank, elem_len, j, m;
    II new_shape[MAX_RANK];
    float *data;
    tensor_t *ft, *at, *out;
    L tmp, item;

    t = evlis(s, t, e);
    if (is_nil(s, t))
        return s->l_err;

    item = car(s, t);

    if (T(item) == TENS)
    {
        /* all elements must be tensors sharing the same rank and shape */
        ft = &s->tensor_heap[ord(item)];
        elem_rank = ft->rank;
        elem_len = ft->len;
        n = 0;
        tmp = t;
        while (!is_nil(s, tmp))
        {
            item = car(s, tmp);
            if (T(item) != TENS)
                return s->l_err;
            at = &s->tensor_heap[ord(item)];
            if (at->rank != elem_rank || at->len != elem_len)
                return s->l_err;
            n++;
            tmp = cdr(s, tmp);
        }
        if (elem_rank + 1 > MAX_RANK)
            return s->l_err;
        new_shape[0] = n;
        for (j = 0; j < elem_rank; j++)
            new_shape[j + 1] = ft->shape[j];
        II new_len = n * elem_len;
        data = malloc(new_len * sizeof(float));
        if (!data)
            abort();
        k = 0;
        tmp = t;
        while (!is_nil(s, tmp))
        {
            item = car(s, tmp);
            at = &s->tensor_heap[ord(item)];
            for (m = 0; m < at->len; m++)
                data[k++] = at->data[m];
            tmp = cdr(s, tmp);
        }
        out = alloc_tensor(s, elem_rank + 1, new_shape, new_len, data);
        free(data);
        return box(TENS, (II)(out - s->tensor_heap));
    }
    else
    {
        /* all elements must be scalars */
        n = 0;
        tmp = t;
        while (!is_nil(s, tmp))
        {
            item = car(s, tmp);
            if (T(item) == TENS)
                return s->l_err;
            n++;
            tmp = cdr(s, tmp);
        }
        data = malloc(n * sizeof(float));
        if (!data)
            abort();
        k = 0;
        tmp = t;
        while (!is_nil(s, tmp))
        {
            item = resolve(s, car(s, tmp), e);
            data[k++] = (float)item;
            tmp = cdr(s, tmp);
        }
        new_shape[0] = n;
        out = alloc_tensor(s, 1, new_shape, n, data);
        free(data);
        return box(TENS, (II)(out - s->tensor_heap));
    }
}

/* gc_tensors: tensor mark/compact/patch phases */
void gc_tensors(lisp_state_t *s)
{
    II i;
    if (s->th == 0)
        return;
    unsigned char mark[MAX_TENSORS];
    memset(mark, 0, s->th);
    for (i = s->sp; i < N; i++)
    {
        L v = s->cell[i];
        if (T(v) == TENS && ord(v) < s->th)
            mark[ord(v)] = 1;
    }
    II remap[MAX_TENSORS];
    II new_th = 0;
    for (i = 0; i < s->th; i++)
    {
        if (mark[i])
        {
            remap[i] = new_th;
            if (i != new_th)
                s->tensor_heap[new_th] = s->tensor_heap[i];
            new_th++;
        }
        else
        {
            free(s->tensor_heap[i].data);
            s->tensor_heap[i].data = NULL;
        }
    }
    for (i = s->sp; i < N; i++)
    {
        L v = s->cell[i];
        if (T(v) == TENS && ord(v) < s->th)
            s->cell[i] = box(TENS, remap[ord(v)]);
    }
    s->th = new_th;
}

/* register all tensor primitives into the instance prim[] table */
void register_tensor_prims(lisp_state_t *s)
{
    register_prim(s, "+",           f_add);
    register_prim(s, "-",           f_sub);
    register_prim(s, "*",           f_mul);
    register_prim(s, "/",           f_div);
    register_prim(s, "shape",       f_shape);
    register_prim(s, "rank",        f_rank);
    register_prim(s, "slice",       f_slice);
    register_prim(s, "first",       f_head);
    register_prim(s, "rest",        f_tail);
    register_prim(s, "tensorp",     f_tensor_p);
    register_prim(s, "matmul",      f_matmul);
    register_prim(s, "@",           f_matmul);
    register_prim(s, "transpose",   f_transpose);
    register_prim(s, "T",           f_transpose);
    register_prim(s, "abs",         f_vabs);
    register_prim(s, "sqrt",        f_vsqrt);
    register_prim(s, "exp",         f_exp);
    register_prim(s, "tanh",        f_tanh);
    register_prim(s, "sin",         f_sin);
    register_prim(s, "cos",         f_cos);
    register_prim(s, "normalize",   f_normalize);
    register_prim(s, "pow",         f_vpow);
    register_prim(s, "zero",        f_zero);
    register_prim(s, "causal-mask", f_causal_mask);
    register_prim(s, "dot",         f_dot);
    register_prim(s, "norm",        f_length);
    register_prim(s, "norm2",       f_length2);
    register_prim(s, "dist",        f_dist);
    register_prim(s, "dist2",       f_dist2);
    register_prim(s, "equalp",      f_veq);
    register_prim(s, "length",      f_list_length);
    register_prim(s, "make-tensor", f_make_tensor);
    register_prim(s, "sum",         f_sum);
    register_prim(s, "amax",        f_amax);
    register_prim(s, "log",         f_log);
    register_prim(s, "softmax",     f_softmax);
    register_prim(s, "layer-norm",  f_layer_norm);
    register_prim(s, "reshape",     f_reshape);
    register_prim(s, "slice-range", f_slice_range);
    register_prim(s, "col-slice",   f_col_slice);
    register_prim(s, "argmax",      f_argmax);
    register_prim(s, "vstack",      f_vstack);
}

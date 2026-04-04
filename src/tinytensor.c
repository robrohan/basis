#define R2_MATHS_IMPLEMENTATION
#include "r2_maths.h"
#include "tinylisp.h"
#include "tinytensor.h"

#include <stdlib.h>
#include <string.h>

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

/* helper: apply a scalar op element-wise, or tensor+tensor, returning new tensor */
L tens_binop(L a, L b, char op)
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

/* if x is a stored s-expression (CONS), evaluate it fully before use.
   this lets quoted expressions like q='(+ 1 2) be used directly in
   arithmetic and tensor literals, including deeply nested cases. */
static L resolve(L x, L e)
{
    return T(x) == CONS ? eval(x, e) : x;
}

static L f_add(L t, L e)
{
    t = evlis(t, e);
    L n = resolve(car(t), e);
    while (!is_nil(t = cdr(t)))
        n = tens_binop(n, resolve(car(t), e), '+');
    return num(n);
}

static L f_sub(L t, L e)
{
    t = evlis(t, e);
    L n = resolve(car(t), e);
    while (!is_nil(t = cdr(t)))
        n = tens_binop(n, resolve(car(t), e), '-');
    return num(n);
}

static L f_mul(L t, L e)
{
    t = evlis(t, e);
    L n = resolve(car(t), e);
    while (!is_nil(t = cdr(t)))
        n = tens_binop(n, resolve(car(t), e), '*');
    return num(n);
}

static L f_div(L t, L e)
{
    t = evlis(t, e);
    L n = resolve(car(t), e);
    while (!is_nil(t = cdr(t)))
        n = tens_binop(n, resolve(car(t), e), '/');
    return num(n);
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

/* (shape t) -> rank-1 tensor of dimension sizes */
static L f_shape(L t, L e)
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

/* (rank t) -> scalar; returns 0 for plain numbers (rank-0 scalars) */
static L f_rank(L t, L e)
{
    L x = car(evlis(t, e));
    if (T(x) != TENS) return 0.0;
    return (L)tensor_heap[ord(x)].rank;
}

/* (slice t i) -> element (scalar) or sub-tensor (row) at index i along axis 0 */
static L f_slice(L t, L e)
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

/* (head t) -> first element or row (sugar for slice 0) */
static L f_head(L t, L e)
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

/* (tail t) -> all elements after the first (rank-1: subvector, rank-2: submatrix) */
static L f_tail(L t, L e)
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

/* (tensor? x) -> #t if x is a tensor */
static L f_tensor_p(L t, L e)
{
    L x = car(evlis(t, e));
    return T(x) == TENS ? tru : nil;
}

/* (matmul A B) / (@ A B) — matrix product */
static L f_matmul(L t, L e)
{
    t = evlis(t, e);
    L xa = car(t);
    L xb = car(cdr(t));
    if (T(xa) != TENS || T(xb) != TENS)
        return err;
    tensor_t *a = &tensor_heap[ord(xa)];
    tensor_t *b = &tensor_heap[ord(xb)];

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

/* (transpose M) -> rank-2 tensor with rows and columns swapped */
static L f_transpose(L t, L e)
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

/* (abs v) — element-wise absolute value; vec4 fast path */
static L f_vabs(L t, L e)
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
static L f_vsqrt(L t, L e)
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

/* (exp x) — e^x; works on scalars and element-wise on tensors */
static L f_exp(L t, L e)
{
    L x = car(evlis(t, e));
    I i;
    if (T(x) != TENS)
        return (L)exp((double)x);
    tensor_t *a = &tensor_heap[ord(x)];
    tensor_t *out = alloc_tensor(a->rank, a->shape, a->len, NULL);
    for (i = 0; i < a->len; i++)
        out->data[i] = expf(a->data[i]);
    return box(TENS, (I)(out - tensor_heap));
}

/* (normalize v) — scale to unit length; vec2/vec4 fast paths */
static L f_normalize(L t, L e){TENS_UNARY_DISP(vec2_normalize, vec4_normalize, vecn_normalize)}

/* (pow v exp) — element-wise v^exp; vec2/vec4 fast paths */
static L f_vpow(L t, L e)
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
static L f_zero(L t, L e)
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
static L f_dot(L t, L e){TENS_BINARY_SCALAR_DISP(vec2_dot, vec4_dot, vecn_dot)}

/* (length v) — Euclidean length → scalar; vec2/vec4 fast paths */
static L f_length(L t, L e){TENS_UNARY_SCALAR_DISP(vec2_length, vec4_length, vecn_length)}

/* (length2 v) — length squared → scalar; vec2/vec4 fast paths */
static L f_length2(L t, L e){TENS_UNARY_SCALAR_DISP(vec2_length_sqrd, vec4_length_sqrd, vecn_length_sqrd)}

/* (dist v1 v2) — Euclidean distance → scalar; vec2/vec4 fast paths */
static L f_dist(L t, L e){TENS_BINARY_SCALAR_DISP(vec2_dist, vec4_dist, vecn_dist)}

/* (dist2 v1 v2) — distance squared -> scalar; vec2/vec4 fast paths */
static L f_dist2(L t, L e){TENS_BINARY_SCALAR_DISP(vec2_dist_sqrd, vec4_dist_sqrd, vecn_dist_sqrd)}

/* (vec= v1 v2) — element-wise equality -> #t or () */
static L f_veq(L t, L e)
{
    t = evlis(t, e);
    L xa = car(t), xb = car(cdr(t));
    if (T(xa) != TENS || T(xb) != TENS)
        return err;
    tensor_t *a = &tensor_heap[ord(xa)];
    tensor_t *b = &tensor_heap[ord(xb)];
    return vecn_equals(a->data, b->data, (int)a->len) ? tru : nil;
}

/* (make-tensor e1 e2 ...) -- runtime backend for [ ] tensor literals */
static L f_make_tensor(L t, L e)
{
    I n, k, elem_rank, elem_len, j, m;
    I new_shape[MAX_RANK];
    float *data;
    tensor_t *ft, *at, *out;
    L tmp, item;

    t = evlis(t, e);
    if (is_nil(t))
        return err;

    item = car(t);

    if (T(item) == TENS)
    {
        /* all elements must be tensors sharing the same rank and shape */
        ft = &tensor_heap[ord(item)];
        elem_rank = ft->rank;
        elem_len = ft->len;
        n = 0;
        tmp = t;
        while (!is_nil(tmp))
        {
            item = car(tmp);
            if (T(item) != TENS)
                return err;
            at = &tensor_heap[ord(item)];
            if (at->rank != elem_rank || at->len != elem_len)
                return err;
            n++;
            tmp = cdr(tmp);
        }
        if (elem_rank + 1 > MAX_RANK)
            return err;
        new_shape[0] = n;
        for (j = 0; j < elem_rank; j++)
            new_shape[j + 1] = ft->shape[j];
        I new_len = n * elem_len;
        data = malloc(new_len * sizeof(float));
        if (!data)
            abort();
        k = 0;
        tmp = t;
        while (!is_nil(tmp))
        {
            item = car(tmp);
            at = &tensor_heap[ord(item)];
            for (m = 0; m < at->len; m++)
                data[k++] = at->data[m];
            tmp = cdr(tmp);
        }
        out = alloc_tensor(elem_rank + 1, new_shape, new_len, data);
        free(data);
        return box(TENS, (I)(out - tensor_heap));
    }
    else
    {
        /* all elements must be scalars */
        n = 0;
        tmp = t;
        while (!is_nil(tmp))
        {
            item = car(tmp);
            if (T(item) == TENS)
                return err;
            n++;
            tmp = cdr(tmp);
        }
        data = malloc(n * sizeof(float));
        if (!data)
            abort();
        k = 0;
        tmp = t;
        while (!is_nil(tmp))
        {
            item = resolve(car(tmp), e);
            data[k++] = (float)item;
            tmp = cdr(tmp);
        }
        new_shape[0] = n;
        out = alloc_tensor(1, new_shape, n, data);
        free(data);
        return box(TENS, (I)(out - tensor_heap));
    }
}

/* gc_tensors: tensor mark/compact/patch phases */
void gc_tensors(void)
{
    I i;
    if (th == 0)
        return;
    unsigned char mark[MAX_TENSORS];
    memset(mark, 0, th);
    for (i = sp; i < N; i++)
    {
        L v = cell[i];
        if (T(v) == TENS && ord(v) < th)
            mark[ord(v)] = 1;
    }
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
    for (i = sp; i < N; i++)
    {
        L v = cell[i];
        if (T(v) == TENS && ord(v) < th)
            cell[i] = box(TENS, remap[ord(v)]);
    }
    th = new_th;
}

/* register all tensor primitives into the global prim[] table */
void register_tensor_prims(void)
{
    register_prim("+",           f_add);
    register_prim("-",           f_sub);
    register_prim("*",           f_mul);
    register_prim("/",           f_div);
    register_prim("shape",       f_shape);
    register_prim("rank",        f_rank);
    register_prim("slice",       f_slice);
    register_prim("head",        f_head);
    register_prim("tail",        f_tail);
    register_prim("tensor?",     f_tensor_p);
    register_prim("matmul",      f_matmul);
    register_prim("@",           f_matmul);
    register_prim("transpose",   f_transpose);
    register_prim("T",           f_transpose);
    register_prim("abs",         f_vabs);
    register_prim("sqrt",        f_vsqrt);
    register_prim("exp",         f_exp);
    register_prim("normalize",   f_normalize);
    register_prim("pow",         f_vpow);
    register_prim("zero",        f_zero);
    register_prim("dot",         f_dot);
    register_prim("length",      f_length);
    register_prim("length2",     f_length2);
    register_prim("dist",        f_dist);
    register_prim("dist2",       f_dist2);
    register_prim("vec=",        f_veq);
    register_prim("make-tensor", f_make_tensor);
}

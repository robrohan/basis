# Tensp Language Design

Tensp is a Lisp-like language where tensors (scalars, vectors, matrices, and
higher-dimensional arrays) are the fundamental data structure, replacing the
cons cell / linked list of traditional Lisp.

## Motivation

Traditional Lisp is built on a single primitive: the cons pair. Everything —
code and data alike — is a tree of cons cells. Tensp keeps that uniformity
(homoiconicity) but replaces the cons pair with a tensor, making
multi-dimensional numeric data a first-class citizen rather than a library
concern.

## Core Idea: Homoiconicity Preserved

In standard Lisp, code is data because both are lists:

```lisp
'(+ 1 2)   ; a list that is also an expression
```

In Tensp, code is data because both are tensors:

```
'(+, [1, 2])   ; a tensor that is also an expression
```

The operator is the head element, the argument tensor follows. Structure and
computation are still the same thing.

## Tensor Hierarchy

A tensor is the general term for an n-dimensional array of values. All of the
following are tensors:

| Name    | Rank | Example           |
|---------|------|-------------------|
| Scalar  | 0    | `42`              |
| Vector  | 1    | `[1, 2, 3]`       |
| Matrix  | 2    | `[[1,2],[3,4]]`   |
| Tensor  | N    | `[[[1,2],[3,4]]]` |

A scalar is a rank-0 tensor. A vector is a rank-1 tensor. The language treats
them all uniformly.

## Syntax

### Tensor Literals

Tensors are written with square brackets. Elements are separated by commas or
spaces (commas are treated as whitespace, following Common Lisp convention).

```
[1, 2, 3]          ; vector (rank 1, shape [3])
[[1,2],[3,4]]      ; matrix (rank 2, shape [2,2])
[[[1,2],[3,4]],
 [[5,6],[7,8]]]    ; rank-3 tensor, shape [2,2,2]
```

Nesting depth determines rank. Row vs column structure is expressed through
nesting, not a separator character (the semicolon is reserved as a comment
character in Lisp syntax).

### Expression Syntax

An expression is an operator followed by a tensor of arguments:

```
(op, [arg1, arg2, ...])
```

Arguments can themselves be expressions:

```
(+, [1, 2, (*, [3, 4])])
```

This evaluates inside-out, exactly like standard Lisp: `(*, [3, 4])` resolves
to `12` first, giving `(+, [1, 2, 12])` = `15`.

### BNF Grammar

```bnf
program    ::= expr*
expr       ::= atom
             | number
             | "'" expr
             | "(" op "," tensor ")"
             | "(" op "," expr ")"
tensor     ::= "[" elements "]"
elements   ::= element ("," element)*
             | element (" " element)*
element    ::= expr | number | atom | tensor
op         ::= atom
atom       ::= [a-zA-Z+\-*/<>=!?][a-zA-Z0-9+\-*/<>=!?]*
number     ::= [0-9]+ ("." [0-9]+)?
```

Commas are treated as whitespace by the scanner so `[1, 2, 3]` and `[1 2 3]`
are identical.

## Primitive Operations

### Structure Primitives

These replace `car`/`cdr` for navigating tensors:

| Primitive        | Description                                      |
|------------------|--------------------------------------------------|
| `(shape, [t])`   | Returns the shape as a vector, e.g. `[2, 3]`    |
| `(rank, [t])`    | Returns the number of dimensions (scalar)        |
| `(slice, [t, i])`| Element or sub-tensor at index `i` along axis 0 |
| `(head, [t])`    | First element along axis 0 (sugar for slice 0)  |
| `(tail, [t])`    | All but first element along axis 0              |

`(pair? x)` from tinylisp becomes `(> (rank, [t]) 0)`.

### Arithmetic Primitives

Arithmetic operates element-wise on tensors, scalar-broadcast following
standard rules:

```
(+, [[1,2],[3,4]], 1)    ; broadcast: adds 1 to every element
(*, [[1,2],[3,4]], [[5,6],[7,8]])  ; element-wise multiply
```

Matrix multiplication is a distinct primitive:

```
(matmul, [A, B])
```

### Kept from tinylisp

```
eval, quote, if, cond, let*, lambda, define, and, or, not, eq?, <
```

These work unchanged. `cons` / `car` / `cdr` / `pair?` are replaced or
aliased to tensor equivalents.

## Internal Representation

### NaN-Boxing Extension

Tinylisp uses NaN-boxing to tag values in a `double`. A new tag `TENS` is
added alongside `ATOM`, `PRIM`, `CONS`, `CLOS`, `NIL`:

```c
I TENS = 0x7ffd;
```

A TENS value's ordinal points into a separate tensor heap (distinct from the
atom heap) where tensor metadata and data are stored.

### Tensor Storage

```c
typedef struct {
    unsigned char rank;      // number of dimensions
    unsigned int  shape[8];  // size of each dimension (max rank 8)
    unsigned int  len;       // total number of elements
    float        *data;      // flat row-major float array
} tensor_t;
```

### Using r2_maths.h

`vendor/r2_maths.h` provides fixed-size types used as efficient backing for
common cases:

| Tensp tensor shape | r2_maths type | Notes                      |
|--------------------|---------------|----------------------------|
| `[2]`              | `vec2`        | 2-element float vector     |
| `[3]` or `[4]`     | `vec4`        | vec3 is backed by vec4     |
| `[3,3]`            | `mat3`        | 3x3 float matrix           |
| `[4,4]`            | `mat4`        | 4x4 float matrix           |
| arbitrary          | `tensor_t`    | general case with `mat_mul`|

The `mat_mul(m1, m2, r1, c1, r2, c2, out)` function in r2_maths.h handles
arbitrary-size matrix multiplication and is used to back `matmul`.

## Evaluation Model

```
eval(x, e):
  if x is ATOM  → look up in environment e
  if x is TENS  → return as-is (tensors are self-evaluating)
  if x is CONS  → apply(eval(car(x), e), cdr(x), e)
  otherwise     → return x
```

A tensor literal in code is self-evaluating, just like a number. An
expression `(op, [args])` is still a CONS at the top level (the `(...)` part),
with a TENS value as the argument. This means the existing `eval`/`apply`
machinery requires minimal changes.

## Example Programs

```lisp
; dot product of two vectors
(define dot (lambda (a b)
  (reduce, [(+, 0), (*,  [a, b])])))

; 2x2 matrix multiply
(define A [[1,2],[3,4]])
(define B [[5,6],[7,8]])
(matmul, [A, B])

; map a function over a vector
(map, [(lambda (x) (*,  [x, 2])), [1, 2, 3, 4]])
```

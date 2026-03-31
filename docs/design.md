# Basis Language Design

Basis is a Lisp-like language where tensors (scalars, vectors, matrices, and
higher-dimensional arrays) are the fundamental data structure, replacing the
cons cell / linked list of traditional Lisp.

## Motivation

Traditional Lisp is built on a single primitive: the cons pair. Everything —
code and data alike — is a tree of cons cells. Basis keeps that uniformity
(homoiconicity) but replaces the cons pair with a tensor, making
multi-dimensional numeric data a first-class citizen rather than a library
concern.

## Core Idea: Homoiconicity Preserved

In standard Lisp, code is data because both are lists:

```lisp
'(+ 1 2)   ; a list that is also an expression
```

In Basis, code is data because both are tensors:

```
'(+ [1 2])   ; a tensor that is also an expression
```

The operator is the head element, the argument tensor follows. Structure and
computation are still the same thing.

## Tensor Hierarchy

A tensor is the general term for an n-dimensional array of values. All of the
following are tensors:

| Name    | Rank | Example           |
|---------|------|-------------------|
| Scalar  | 0    | `42`              |
| Vector  | 1    | `[1 2 3]`         |
| Matrix  | 2    | `[[1 2] [3 4]]`   |
| Tensor  | N    | `[[[1 2] [3 4]]]` |

A scalar is a rank-0 tensor. A vector is a rank-1 tensor. The language treats
them all uniformly.

## Syntax

### Tensor Literals

Tensors are written with square brackets. Elements are separated by whitespace.

```
[1 2 3]                  ; vector (rank 1, shape [3])
[[1 2] [3 4]]            ; matrix (rank 2, shape [2 2])
[[[1 2] [3 4]]
 [[5 6] [7 8]]]          ; rank-3 tensor, shape [2 2 2]
```

Nesting depth determines rank. Row vs column structure is expressed through
nesting, not a separator character.

### Expression Syntax

An expression is a parenthesised sequence: the first element is the operator,
the rest are arguments:

```
(op arg1 arg2 ...)
```

Tensor literals can appear directly as arguments:

```
(+ [1 2 3] [4 5 6])     ; element-wise add two vectors
(dot [1 2 3] [4 5 6])   ; dot product
```

Expressions evaluate inside-out, exactly like standard Lisp:

```
(+ 1 2 (* 3 4))   ; (* 3 4) → 12, then (+ 1 2 12) → 15
```

### BNF Grammar

```bnf
program  ::= expr*
expr     ::= atom
           | number
           | "'" expr
           | "(" expr* ")"
           | "[" expr* "]"
atom     ::= <UTF-8 sequence not containing ( ) [ ] or whitespace>
number   ::= "-"? [0-9]+ ("." [0-9]+)?
```

Notes:
- `[expr*]` rank is determined at eval time from the shapes of the evaluated elements:
  all-scalar elements → rank-1 vector; all equal-shaped rank-k elements → rank-(k+1) tensor.

## Primitive Operations

### Structure Primitives

These replace `car`/`cdr` for navigating tensors:

| Primitive       | Description                                      |
|-----------------|--------------------------------------------------|
| `(shape t)`     | Returns the shape as a vector, e.g. `[2 3]`     |
| `(rank t)`      | Returns the number of dimensions (scalar)        |
| `(slice t i)`   | Element or sub-tensor at index `i` along axis 0 |
| `(head t)`      | First element along axis 0 (sugar for slice 0)  |
| `(tail t)`      | All but first element along axis 0              |

`(tensor? x)` tests whether a value is a tensor.

### Arithmetic Primitives

Arithmetic operates element-wise on tensors, scalar-broadcast following
standard rules:

```
(+ [[1 2] [3 4]] 1)               ; broadcast: adds 1 to every element
(* [[1 2] [3 4]] [[5 6] [7 8]])   ; element-wise multiply
```

Matrix multiplication is a distinct primitive:

```
(matmul A B)
(@ A B)      ; alias
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

All tensors are stored as `tensor_t` with a flat `float*` data array.
`vendor/r2_maths.h` provides fixed-size math functions (`vec2_dot`,
`vec4_normalize`, etc.) that operate on the same `float*` memory layout —
they are just unions over `float[N]`, not a separate representation. For
common small sizes (len=2, len=4) these are dispatched as a performance
shortcut; all other sizes fall through to the generic `vecn_*` loop
functions. `mat_mul` handles arbitrary-size matrix multiplication.

## Evaluation Model

```
eval(x, e):
  if x is ATOM  → look up in environment e
  if x is TENS  → return as-is (tensors are self-evaluating)
  if x is CONS  → apply(eval(car(x), e), cdr(x), e)
  otherwise     → return x
```

A tensor literal in code is self-evaluating, just like a number. An
expression `(op [args])` is still a CONS at the top level (the `(...)` part),
with a TENS value as the argument. This means the existing `eval`/`apply`
machinery requires minimal changes.

## Example Programs

```lisp
; dot product of two vectors
(dot [1 2 3] [4 5 6])
; => 32

; 2x2 matrix multiply
(define A [[1 2] [3 4]])
(define B [[5 6] [7 8]])
(@ A B)
; => [[19 22] [43 50]]

; scale a vector with a lambda
(define scale (lambda (v s) (* v s)))
(scale [1 2 3] 10)
; => [10 20 30]
```

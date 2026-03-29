# Tensp

A Lisp interpreter where tensors (scalars, vectors, matrices, N-dimensional arrays) are the fundamental data type. Code and data share the same tensor structure, preserving Lisp's homoiconicity.

## Syntax

```
(op arg1 arg2)          ; standard Lisp call
[1 2 3]                 ; vector literal
[[1 2] [3 4]]           ; matrix literal (2x3, etc.)
(op, [arg1, arg2])      ; commas are whitespace — same as above
```

## Build

```sh
make fetch      # download vendor headers
make build      # build the REPL
make test       # build and run unit tests
make run        # start the REPL
```

## Primitives

### Core Lisp

| Primitive | Example | Description |
|---|---|---|
| `quote` | `(quote foo)` | Return expression unevaluated |
| `eval` | `(eval x)` | Evaluate expression |
| `cons` | `(cons 1 2)` | Construct a pair |
| `car` | `(car p)` | First element of pair |
| `cdr` | `(cdr p)` | Second element of pair |
| `eq?` | `(eq? x y)` | Structural equality |
| `pair?` | `(pair? x)` | True if x is a cons pair |
| `if` | `(if c t f)` | Conditional |
| `cond` | `(cond (c1 e1) (c2 e2))` | Multi-branch conditional |
| `and` | `(and x y)` | Logical and |
| `or` | `(or x y)` | Logical or |
| `not` | `(not x)` | Logical not |
| `lambda` | `(lambda (x) (* x x))` | Anonymous function |
| `define` | `(define sq (lambda (x) (* x x)))` | Bind name in environment |
| `let*` | `(let* (x 1) (y 2) (+ x y))` | Sequential local bindings |
| `int` | `(int 3.9)` | Truncate to integer |
| `<` | `(< 1 2)` | Less than |

### Arithmetic (scalars and tensors)

All four operators work on scalars, tensors, and mixed scalar/tensor (broadcast).

| Primitive | Example | Description |
|---|---|---|
| `+` | `(+ [1 2] [3 4])` | Add |
| `-` | `(- [5 6] [1 2])` | Subtract |
| `*` | `(* [1 2] 3)` | Multiply / scalar broadcast |
| `/` | `(/ [6 8] 2)` | Divide / scalar broadcast |

### Tensor constructors and inspection

| Primitive | Example | Result | Description |
|---|---|---|---|
| `zero` | `(zero 4)` | `[0 0 0 0]` | Rank-1 zero tensor of length n |
| `shape` | `(shape [[1 2][3 4]])` | `[2 2]` | Dimension sizes as a vector |
| `rank` | `(rank [[1 2][3 4]])` | `2` | Number of dimensions |
| `slice` | `(slice [10 20 30] 1)` | `20` | Element or row at index i |
| `tensor?` | `(tensor? [1 2])` | `#t` | True if x is a tensor |

### Matrix operations

| Primitive | Alias | Example | Description |
|---|---|---|---|
| `matmul` | `@` | `(@ [[1 2][3 4]] [[5 6][7 8]])` | Matrix multiply (mat×mat, mat×vec, vec×mat) |
| `transpose` | `T` | `(T [[1 2 3][4 5 6]])` | Swap rows and columns |

### Vector math

| Primitive | Example | Result | Description |
|---|---|---|---|
| `dot` | `(dot [1 2 3] [4 5 6])` | `32` | Dot product → scalar |
| `length` | `(length [3 4])` | `5` | Euclidean length → scalar |
| `length2` | `(length2 [3 4])` | `25` | Length squared → scalar |
| `dist` | `(dist [0 0] [3 4])` | `5` | Distance between two points → scalar |
| `dist2` | `(dist2 [0 0] [3 4])` | `25` | Distance squared → scalar |
| `normalize` | `(normalize [3 4])` | `[0.6 0.8]` | Scale to unit length |
| `abs` | `(abs [-3 1 -2])` | `[3 1 2]` | Element-wise absolute value |
| `sqrt` | `(sqrt [4 9 16])` | `[2 3 4]` | Element-wise square root |
| `pow` | `(pow [2 3 4] 2)` | `[4 9 16]` | Element-wise power |
| `vec=` | `(vec= [1 2] [1 2])` | `#t` | Element-wise equality |

## Examples

```lisp
; dot product of two vectors
(dot [1 2 3] [4 5 6])
; => 32

; normalize a vector
(normalize [3 4])
; => [0.6 0.8]

; matrix multiply
(@ [[1 2] [3 4]] [[5 6] [7 8]])
; => [[19 22] [43 50]]

; transpose
(T [[1 2 3] [4 5 6]])
; => [[1 4] [2 5] [3 6]]

; chain: A * A^T
(@ [[1 2] [3 4]] (T [[1 2] [3 4]]))
; => [[5 11] [11 25]]

; lambda over tensors
(define scale (lambda (v s) (* v s)))
(scale [1 2 3] 10)
; => [10 20 30]
```

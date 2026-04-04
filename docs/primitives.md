# Basis Primitives Reference

## Core Lisp

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
| `define` | `(define sq (lambda (x) (* x x)))` | Create a new binding in the environment |
| `set!` | `(set! x 42)` | Update an existing binding in place; error if unbound |
| `let*` | `(let* (x 1) (y 2) (+ x y))` | Sequential local bindings |
| `int` | `(int 3.9)` | Truncate to integer |
| `<` | `(< 1 2)` | Less than |
| `gc` | `(gc)` | Force a garbage collection cycle |

## Arithmetic

All four operators work on scalars, tensors, and mixed scalar/tensor (broadcast).

| Primitive | Example | Description |
|---|---|---|
| `+` | `(+ [1 2] [3 4])` | Add |
| `-` | `(- [5 6] [1 2])` | Subtract |
| `*` | `(* [1 2] 3)` | Multiply / scalar broadcast |
| `/` | `(/ [6 8] 2)` | Divide / scalar broadcast |

## Tensor Constructors and Inspection

| Primitive | Example | Result | Description |
|---|---|---|---|
| `zero` | `(zero 4)` | `[0 0 0 0]` | Rank-1 zero tensor of length n |
| `shape` | `(shape [[1 2][3 4]])` | `[2 2]` | Dimension sizes as a vector |
| `rank` | `(rank [[1 2][3 4]])` | `2` | Number of dimensions. Returns 0 for plain numbers (scalars). |
| `slice` | `(slice [10 20 30] 1)` | `20` | Element or row at index i |
| `head` | `(head [10 20 30])` | `10` | First element or row |
| `tail` | `(tail [10 20 30])` | `[20 30]` | All elements after the first |
| `tensor?` | `(tensor? [1 2])` | `#t` | True if x is a tensor |

## Matrix Operations

| Primitive | Alias | Example | Description |
|---|---|---|---|
| `matmul` | `@` | `(@ [[1 2][3 4]] [[5 6][7 8]])` | Matrix multiply (mat×mat, mat×vec, vec×mat) |
| `transpose` | `T` | `(T [[1 2 3][4 5 6]])` | Swap rows and columns |

## Vector Math

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

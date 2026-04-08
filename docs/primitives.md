# Basis Primitives Reference

## Core Lisp

| Primitive | Example | Description |
|---|---|---|
| `quote` | `(quote foo)` | Return expression unevaluated |
| `eval` | `(eval x)` | Evaluate expression |
| `cons` | `(cons 1 2)` | Construct a pair |
| `car` | `(car p)` | First element of pair |
| `cdr` | `(cdr p)` | Second element of pair |
| `equal` | `(equal x y)` | Structural equality: tensors compare element-wise, atoms by identity, numbers by value |
| `consp` | `(consp x)` | True if x is a cons pair (non-empty list) |
| `if` | `(if c t f)` | Conditional |
| `cond` | `(cond (c1 e1) (c2 e2))` | Multi-branch conditional |
| `and` | `(and x y)` | Logical and |
| `or` | `(or x y)` | Logical or |
| `not` | `(not x)` | Logical not |
| `lambda` | `(lambda (x) (* x x))` | Anonymous function |
| `define` | `(define sq (lambda (x) (* x x)))` | Create a new binding in the environment (kept for backward compat) |
| `defparameter` | `(defparameter pi 3.14159)` | Define a global variable (CL style) |
| `defvar` | `(defvar count 0)` | Define a global variable (CL style) |
| `defun` | `(defun sq (x) (* x x))` | Define a named function (CL style) |
| `setq` | `(setq x 42)` | Update an existing binding in place; error if unbound (CL setq) |
| `let*` | `(let* (x 1) (y 2) (+ x y))` | Sequential local bindings |
| `truncate` | `(truncate 3.9)` | Truncate to integer part (CL truncate) |
| `<` | `(< 1 2)` | Less than |
| `>` | `(> 2 1)` | Greater than |
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
| `make-tensor` | `(make-tensor 3 0.0)` | `[0 0 0]` | Create rank-1 tensor of n elements filled with value |
| `shape` | `(shape [[1 2][3 4]])` | `[2 2]` | Dimension sizes as a vector |
| `rank` | `(rank [[1 2][3 4]])` | `2` | Number of dimensions. Returns 0 for plain numbers (scalars). |
| `slice` | `(slice [10 20 30] 1)` | `20` | Element or row at index i along axis 0 |
| `slice-range` | `(slice-range M 2 5)` | `[row2 row3 row4]` | Sub-matrix of rows [start, end) along axis 0 |
| `col-slice` | `(col-slice M i)` | row vector | Extract column i as a rank-1 vector (used for embedding lookup) |
| `first` | `(first [10 20 30])` | `10` | First element or row |
| `rest` | `(rest [10 20 30])` | `[20 30]` | All elements after the first |
| `reshape` | `(reshape [1 2 3 4] [2 2])` | `[[1 2][3 4]]` | Change shape without moving data; new shape given as tensor |
| `vstack` | `(vstack A B)` | matrix | Stack two tensors row-wise; rank-1 inputs treated as single rows |
| `tensorp` | `(tensorp [1 2])` | `#t` | True if x is a tensor |

## Matrix Operations

| Primitive | Alias | Example | Description |
|---|---|---|---|
| `matmul` | `@` | `(@ [[1 2][3 4]] [[5 6][7 8]])` | Matrix multiply (mat×mat, mat×vec, vec×mat) |
| `transpose` | `T` | `(T [[1 2 3][4 5 6]])` | Swap rows and columns |

## Vector Math

| Primitive | Example | Result | Description |
|---|---|---|---|
| `dot` | `(dot [1 2 3] [4 5 6])` | `32` | Dot product → scalar |
| `norm` | `(norm [3 4])` | `5` | Euclidean norm (length) → scalar |
| `norm2` | `(norm2 [3 4])` | `25` | Norm squared (length squared) → scalar |
| `dist` | `(dist [0 0] [3 4])` | `5` | Distance between two points → scalar |
| `dist2` | `(dist2 [0 0] [3 4])` | `25` | Distance squared → scalar |
| `normalize` | `(normalize [3 4])` | `[0.6 0.8]` | Scale to unit length |
| `abs` | `(abs [-3 1 -2])` | `[3 1 2]` | Element-wise absolute value |
| `sqrt` | `(sqrt [4 9 16])` | `[2 3 4]` | Element-wise square root |
| `pow` | `(pow [2 3 4] 2)` | `[4 9 16]` | Element-wise power |
| `equalp` | `(equalp [1 2] [1 2])` | `#t` | Element-wise equality (CL equalp for arrays) |
| `sin` | `(sin [0 1.57])` | `[0 1]` | Element-wise sine (radians) |
| `cos` | `(cos [0 3.14])` | `[1 -1]` | Element-wise cosine (radians) |
| `exp` | `(exp [0 1])` | `[1 2.718]` | Element-wise e^x |
| `log` | `(log [1 2.718])` | `[0 1]` | Element-wise natural log |
| `tanh` | `(tanh [0 1])` | `[0 0.762]` | Element-wise hyperbolic tangent |

## List Operations

| Primitive | Example | Result | Description |
|---|---|---|---|
| `length` | `(length '(a b c))` | `3` | Number of elements in a Lisp list (CL length) |

## Reductions

| Primitive | Example | Result | Description |
|---|---|---|---|
| `sum` | `(sum [1 2 3])` | `6` | Sum all elements → scalar |
| `amax` | `(amax [3 1 4 1 5])` | `5` | Maximum element value → scalar |
| `argmax` | `(argmax [0.1 0.7 0.2])` | `1` | Index of the maximum element → scalar |

## Neural Network Ops

| Primitive | Example | Description |
|---|---|---|
| `softmax` | `(softmax logits)` | Numerically stable softmax over a rank-1 or rank-2 tensor |
| `layer-norm` | `(layer-norm x eps)` | Layer normalisation: subtract mean, divide by std; apply scale/shift separately |
| `causal-mask` | `(causal-mask n)` | (n x n) lower-triangular mask: 0.0 on/below diagonal, -1e9 above; used before softmax in attention |

## Symbolic

| Primitive | Example | Description |
|---|---|---|
| `match` | `(match '(?x is ?y) '(sky is blue))` | Unify pattern against data; `?`-prefixed atoms are variables; returns bindings alist or `ERR` |

## I/O and Files

| Primitive | Example | Description |
|---|---|---|
| `print` | `(print x)` | Print x followed by newline; returns x |
| `load` | `(load "file.lisp")` | Evaluate a Lisp file; accepts string or atom path |

## GGUF Model Loading

GGUF is the binary format used by llama.cpp and related tools to distribute quantised model weights.

| Primitive | Example | Description |
|---|---|---|
| `load-gguf` | `(load-gguf "models/gpt2.Q4_0.gguf")` | Load all tensors from a GGUF file into the global environment; each tensor is bound to its GGUF name as an atom (e.g. `token_embd.weight`) |
| `load-gguf-vocab` | `(load-gguf-vocab "models/gpt2.Q4_0.gguf")` | Load the BPE vocabulary and merge rules from the GGUF metadata; required before calling tokenizer primitives |

## Tokenizer

These primitives implement GPT-2 Byte Pair Encoding (BPE). `load-gguf-vocab` must be called first.

| Primitive | Example | Description |
|---|---|---|
| `tokenize` | `(tokenize "hello world")` | Encode a string to a rank-1 tensor of integer token IDs |
| `detokenize` | `(detokenize toks)` | Decode a rank-1 tensor of token IDs back to a string |
| `token->str` | `(token->str 15496)` | Decode a single token ID to a string |

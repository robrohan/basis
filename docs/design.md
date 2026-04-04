---
Project: Basis
Date: 2026-04-01
---

# Basis Language Design


## 1. Considerations


#### 1.1 Assumptions

Basis is a Lisp-like language where tensors (scalars, vectors, matrices, and higher-dimensional arrays) are the fundamental data structure, replacing the cons cell / linked list of traditional Lisp.

Traditional Lisp is built on a single primitive: the cons pair. Everything, code and data alike, is a tree of cons cells. Basis keeps that uniformity (homoiconicity) but tries to replace the cons pair with a tensor, making multi-dimensional numeric data a first-class citizen rather than a library concern.

The core assumption is that homoiconicity can be preserved with tensors as the base type. In standard Lisp, code is data because both are lists:

```lisp
'(+ 1 2)   ; a list that is also an expression
```

In Basis, code is data because both are tensors:

```lisp
'(+ [1 2])   ; a tensor that is also an expression
```

The operator is the head element, the argument tensor follows. Structure and computation are still the same thing. Hopefully, this will lead to intresting call graphs where one can mix nerual network weights with the ablity to add runtime inspection and modifiation.

#### 1.2 Constraints

- All numeric values are stored as single-precision `float32` in tensor data arrays. Scalars (rank-0 values) are represented as IEEE 754 `double` via NaN-boxing and converted to `float32` when placed into a tensor.
- NaN-boxing requires a 64-bit `double` — the tag bits occupy bits 48–63 of the NaN payload. `float` (32-bit) does not have enough NaN payload to hold both a tag and an ordinal, so the `L` type must remain `double`.
- The tensor heap is a fixed-size pool (`MAX_TENSORS = 0x2000`). Exceeding this limit aborts. Long-running programs must rely on garbage collection to reclaim dead tensors.
- The cons cell / atom heap is a shared fixed-size array (`N = 0x16000` cells). The atom heap grows upward from index 0; the cons stack grows downward from index N. If they meet, the interpreter aborts.
- Maximum tensor rank is `MAX_RANK = 8` dimensions.

#### 1.3 System Environment

Basis is built on two vendored single-header libraries:

- **`vendor/r2_maths.h`** — provides `vec2`, `vec4`, and `vecn_*` / `mat_*` operations over flat `float*` arrays. Tensor arithmetic dispatches to vec2/vec4 fast paths for common small sizes and falls back to generic `vecn_*` loop functions for all other sizes.
- **`vendor/r2_strings.h`** — provides UTF-8 utilities (`utf8_len`, `str_to_utf8`, `s8` / `rune` types). The scanner uses `utf8_len` to correctly tokenize multi-byte atoms (emoji, Unicode symbols). String type support will use `s8`/`rune` for character-aware operations.

The interpreter targets POSIX platforms (macOS, Linux). File mode uses `dup2` to redirect stdin to the input file. The REPL uses `getchar` directly.


## 2. Architecture


#### 2.1 Overview

The interpreter is a NaN-boxed tinylisp extended with a tensor heap. The value type `L` is a 64-bit `double`. High-order bits 48–63 encode a tag; the low 32 bits encode an ordinal (index into a heap or numeric value). Six tags are defined:

| Tag    | Hex    | Meaning                                              |
|--------|--------|------------------------------------------------------|
| `ATOM` | 0x7ff8 | Index into the atom string heap                      |
| `PRIM` | 0x7ff9 | Index into the primitive function table              |
| `CONS` | 0x7ffa | Index into the cons cell stack                       |
| `CLOS` | 0x7ffb | Index into the cons cell stack (closure)             |
| `NIL`  | 0x7ffc | The empty list `()`                                  |
| `TENS` | 0x7ffd | Index into the tensor heap                           |
| `STR`  | 0x7ffe | Reserved for string literals (not yet implemented)   |

Untagged doubles are plain IEEE 754 numbers (scalars).

Evaluation follows the standard tinylisp model extended for tensors:

```
eval(x, e):
  if x is ATOM  -> look up in environment e
  if x is TENS  -> return as-is (tensors are self-evaluating)
  if x is CONS  -> apply(eval(car(x), e), cdr(x), e)
  otherwise     -> return x (number, NIL, PRIM, CLOS)
```

A tensor literal in code is self-evaluating, just like a number. An expression `(op [args])` is still a CONS at the top level, with a TENS value as the argument — the existing `eval`/`apply` machinery requires minimal changes.

#### 2.2 Component Diagrams

```
┌─────────────────────────────────────────────────┐
│                   cell[N]                        │
│                                                  │
│  [0 .. hp)        atom strings (C strings)       │
│  [hp .. sp<<3)    free                           │
│  [sp .. N)        cons stack (grows downward)    │
└─────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────┐
│              tensor_heap[MAX_TENSORS]            │
│  [0 .. th)        live tensor_t structs          │
│  each tensor_t    → malloc'd float* data array   │
└─────────────────────────────────────────────────┘
```

The atom heap and cons stack share `cell[]`. The tensor heap is a separate fixed pool; tensor data arrays are individually `malloc`'d. Garbage collection compacts the tensor heap and resets the cons stack to `ord(env)`.

#### 2.3 Class Diagrams

```c
/* NaN-boxed Lisp value — all interpreter values are this type */
typedef double L;

/* Tensor — rank-N array of floats */
typedef struct {
    I     rank;           /* number of dimensions (max MAX_RANK) */
    I     shape[MAX_RANK];/* size along each dimension           */
    I     len;            /* total elements (product of shape)   */
    float *data;          /* heap-allocated flat row-major array */
} tensor_t;

/* Primitive function entry */
struct prims {
    const char *s;        /* name as seen in Lisp                */
    L (*f)(L, L);         /* C function: (arg-list, environment) */
};
```

#### 2.4 Sequence Diagrams

**REPL cycle:**

```
user input
    -> scan()       tokenise one token into buf[]
    -> parse()      build Lisp expression (CONS tree / TENS / ATOM / number)
    -> eval(x, env) reduce expression in global environment
    -> print(result)
    -> gc()         reset cons stack to ord(env), compact tensor heap
    -> repeat
```

**File mode cycle:**

```
open file -> dup2 onto stdin
    -> while scan():
        parse() → eval() → print()
    -> fclose
```

*(gc() is not currently called between file expressions — see constraints / known issues.)*

#### 2.5 Deployment Diagrams

Basis is a single statically-linked CLI binary. No runtime dependencies beyond libc/libm.

```
source (*.lisp)
    -> basis -f file.lisp    file mode: evaluate all expressions
    -> basis                 REPL mode: interactive read-eval-print loop
```

#### 2.6 Other Diagrams

**Tensor hierarchy:**

| Name   | Rank | Example              |
|--------|------|----------------------|
| Scalar | 0    | `42`                 |
| Vector | 1    | `[1 2 3]`            |
| Matrix | 2    | `[[1 2] [3 4]]`      |
| Tensor | N    | `[[[1 2] [3 4]] ...]`|

**Arithmetic dispatch:**

```
operand types       dispatch
─────────────────────────────────────────────────────
tensor  op tensor   element-wise; shapes must match
scalar  op tensor   broadcast scalar to all elements
tensor  op scalar   broadcast scalar to all elements
scalar  op scalar   plain double arithmetic
```

Matrix multiply (`@` / `matmul`) is always a distinct operation and does not broadcast.


## 3. User Interface Design

Basis has no graphical interface. The user interface is the language syntax itself.

### Syntax

#### Tensor Literals

Tensors are written with square brackets. Elements are separated by whitespace.

```lisp
[1 2 3]                  ; vector (rank 1, shape [3])
[[1 2] [3 4]]            ; matrix (rank 2, shape [2 2])
[[[1 2] [3 4]]
 [[5 6] [7 8]]]          ; rank-3 tensor, shape [2 2 2]
```

Nesting depth determines rank. Elements inside `[...]` are evaluated at runtime, so expressions are valid:

```lisp
(define x 3)
[(+ x 1) x]   ; => [4 3]
```

#### Expression Syntax

An expression is a parenthesised sequence: the first element is the operator, the rest are arguments:

```
(op arg1 arg2 ...)
```

Expressions evaluate inside-out:

```lisp
(+ 1 2 (* 3 4))   ; (* 3 4) → 12, then (+ 1 2 12) → 15
```

#### Comments

`;` starts a line comment (everything to end of line is ignored), following standard Lisp convention. `;;;;` separator lines are common style for section breaks.

#### BNF Grammar

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
- `[expr*]` rank is determined at eval time from the shapes of the evaluated elements. Scalar elements (including a single one) produce a rank-1 vector. All equal-shaped rank-k elements produce a rank-(k+1) tensor. Plain numbers (not inside `[...]`) are rank-0 scalars.

### Primitive Operations

When working with tensors there are some meta information functions that will be needed.

#### Structure Primitives

| Primitive      | Description                                                         |
|----------------|---------------------------------------------------------------------|
| `(shape t)`    | Returns the shape as a vector, e.g. `[2 3]`                                         |
| `(rank t)`     | Returns the number of dimensions. Returns 0 for plain numbers (scalars).            |
| `(slice t i)`  | Element or sub-tensor at index `i` along axis 0                     |
| `(head t)`     | First element along axis 0 (sugar for slice 0)                      |
| `(tail t)`     | All but first element along axis 0                                  |
| `(tensor? x)`  | Returns `#t` if x is a tensor                                       |

#### Arithmetic Primitives

Arithmetic operates element-wise on tensors with scalar broadcast:

```lisp
(+ [[1 2] [3 4]] 1)               ; broadcast: adds 1 to every element
(* [[1 2] [3 4]] [[5 6] [7 8]])   ; element-wise multiply
```

Matrix multiplication is a distinct primitive:

```lisp
(@ A B)         ; matrix multiply (alias for matmul)
(matmul A B)
(T A)           ; transpose (alias for transpose)
```

#### Mutation

`define` creates a new binding in the global environment. Calling it a second time with the same name does not update the existing binding; it prepends a shadow binding that hides the old one. This is fine for one-off definitions but causes unbounded environment growth in loops.

`set!` is the explicit mutation primitive. It walks the environment, finds the first binding for the given name, and updates its value in place. It returns the new value and errors if the name is not already bound.

```lisp
(define x 1)
(set! x 42)     ; x is now 42, env is unchanged in length
x               ; => 42

(set! y 1)      ; => ERR, y was not defined
```

The convention is: use `define` for the initial binding, use `set!` for all subsequent updates. This keeps intent explicit and prevents silent environment growth in training loops or iteration.

#### Kept from tinylisp

```
eval, quote, if, cond, let*, lambda, define, set!, and, or, not, eq?, <
```

These work unchanged. `cons` / `car` / `cdr` / `pair?` remain available but only operate on cons cells (s-expressions), not tensors.

### Example Programs

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

; unicode atoms work fine
(define 🥧 3.14159)
(* 🥧 2)
; => 6.28318
```

See also: [XOR neural network example](../test_data/xor_net.lisp)


## 4. Appendices and References


#### 4.1 Definitions and Abbreviations

| Term         | Definition                                                                 |
|--------------|----------------------------------------------------------------------------|
| NaN-boxing   | Encoding type tags and ordinals inside the payload bits of IEEE 754 NaN doubles |
| Ordinal      | The low 32 bits of a NaN-boxed value; used as a heap or table index        |
| Homoiconicity| Property where code and data share the same representation                 |
| Rank         | The number of dimensions of a tensor (scalar=0, vector=1, matrix=2, ...)   |
| Shape        | A vector of the sizes along each dimension, e.g. `[2 3]` for a 2×3 matrix |
| REPL         | Read-Eval-Print Loop — interactive interpreter mode                        |
| `hp`         | Heap pointer — top of the atom string heap (grows upward)                  |
| `sp`         | Stack pointer — top of the cons cell stack (grows downward from N)         |
| `th`         | Tensor heap pointer — next free slot in `tensor_heap[]`                    |
| `env`        | The global environment — a linked list of `(name . value)` pairs           |
| Primitive    | A built-in function implemented in C and registered in the `prim[]` table  |

#### 4.2 References

- **tinylisp** — Robert A. van Engelen (2022). Original NaN-boxing Lisp in ~200 lines of C. The core eval/apply/gc machinery Basis is built on.
- **r2_maths.h** — vendored single-header math library providing `vec2`, `vec4`, and `vecn_*` / `mat_*` operations over flat `float*` arrays.
- **r2_strings.h** — vendored single-header UTF-8 string library providing `utf8_len`, `str_to_utf8`, and the `s8`/`rune` types.
- **IEEE 754** — the floating-point standard that makes NaN-boxing possible: quiet NaN values with arbitrary payloads in bits 0–50.

---
Project: Basis
Date: 2026-04-01
---

# Basis Language Design


## 1. Considerations


#### 1.1 Assumptions

Basis is a Lisp-like language where tensors (scalars, vectors, matrices, and higher-dimensional arrays) are a fundamental building block of the language and a first class data structure.

Traditional Lisp is built on a single primitive: the cons pair. Everything, code and data alike, is a tree of cons cells. Basis keeps that uniformity (homoiconicity) but tries to enhance the cons pair with tensors, making multi-dimensional numeric data a first-class citizen rather than a library concern.

The core assumption is that homoiconicity can be preserved with tensors as a base type. In standard Lisp, code is data because both are lists:

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

Basis is built on several vendored libraries (often single-header):

- **`vendor/r2_unit.h`** - provides a simple unit testing framework for the C code.
- **`vendor/r2_maths.h`** — provides `vec2`, `vec4`, and `vecn_*` / `mat_*` operations over flat `float*` arrays. Tensor arithmetic dispatches to vec2/vec4 fast paths for common small sizes and falls back to generic `vecn_*` loop functions for all other sizes.
- **`vendor/r2_strings.h`** — provides UTF-8 utilities (`utf8_len`, `str_to_utf8`, `s8` / `rune` types). The scanner uses `utf8_len` to correctly tokenize multi-byte atoms (emoji, Unicode symbols). String type support will use `s8`/`rune` for character-aware operations.
- **`vendor/gguf/*`** - provides code to read models saved in the _GGUF_ file format.
- **`src/tinylisp.c`** - the base of the lisp interpreter. Taken from the project of the same name (modified).

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
| `STR`  | 0x7ffe | Index into the atom heap (string literal)            |

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
lisp_state_t {
  ┌─────────────────────────────────────────────────┐
  │                   cell[N]                       │
  │                                                 │
  │  [0 .. hp)        atom strings (C strings)      │
  │  [hp .. sp<<3)    free                          │
  │  [sp .. N)        cons stack (grows downward)   │
  └─────────────────────────────────────────────────┘

  ┌─────────────────────────────────────────────────┐
  │              tensor_heap[MAX_TENSORS]           │
  │  [0 .. th)        live tensor_t structs         │
  │  each tensor_t   -> malloc'd float* data array  │
  └─────────────────────────────────────────────────┘
}
```

Both heaps are fields of `lisp_state_t`. The atom heap and cons stack share `cell[]`. The tensor heap is a separate fixed pool; tensor data arrays are individually `malloc`'d. Garbage collection compacts the tensor heap and resets the cons stack to `ord(env)`. Each `lisp_state_t` instance has its own independent copy of both heaps.

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

/* Forward declaration needed because struct prims references lisp_state_t */
typedef struct lisp_state lisp_state_t;

/* Primitive function entry */
struct prims {
    const char *s;                      /* name as seen in Lisp                */
    L (*f)(lisp_state_t *, L, L);       /* C function: (state, arg-list, env)  */
};

/* Per-instance interpreter state — all formerly-global variables */
struct lisp_state {
    L    cell[N];                /* shared NaN-boxed cell array              */
    II   hp;                     /* heap pointer — grows up from 0           */
    II   sp;                     /* stack pointer — grows down from N        */
    L    l_nil, l_tru, l_err;    /* interpreter singletons                   */
    L    l_env;                  /* global environment (linked alist)        */
    char buf[256];               /* scanner token buffer                     */
    int  see;                    /* lookahead character                      */
    FILE *input_stream;          /* current input (stdin or loaded file)     */
    tensor_t tensor_heap[MAX_TENSORS]; /* fixed tensor pool                  */
    II       th;                 /* next free tensor slot                    */
    struct prims prim[MAX_PRIMS];/* registered primitive table               */
    int          prim_count;     /* number of registered primitives          */
};

lisp_state_t *lisp_state_new(void);   /* allocate and zero-initialise        */
void          lisp_state_free(lisp_state_t *s); /* free tensors then struct  */
```

Every public API function takes `lisp_state_t *s` as its first argument. This allows multiple independent interpreter instances to coexist in the same process — the intended use case is the proxy server design where model weights are loaded once into a shared read-only `universe_t` and a pool of `lisp_state_t` instances handles concurrent HTTP requests.

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

#### 2.5 Deployment Diagrams

Basis is a single statically-linked CLI binary. No runtime dependencies beyond libc/libm.

```
source (*.lisp)
    -> basis -f file.lisp    file mode: evaluate all expressions
    -> basis file.lisp       file mode: evaluate all expressions
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

Currently, basis has no graphical interface. The user interface is the language syntax itself and the REPL.

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
[(+ x 1) x]              ; => [4 3]
```

#### String Literals

Strings are written with double quotes. They are self-evaluating and stored as raw UTF-8 bytes in the atom heap with the `STR` NaN-box tag.

```lisp
"hello world"          ; string literal
(print "hello world")  ; prints: hello world
```

Strings can be passed to primitives that accept path arguments (`load`, `load-gguf`, `load-gguf-vocab`) or printed directly. The `print` primitive outputs raw bytes without surrounding quotes; displayed strings should render correctly on UTF-8 terminals.

Rune-aware (UTF-8 aware) string operations (`string-length`, `string-ref`, `string-append`) are planned but not yet implemented.

#### Expression Syntax

An expression is a parenthesised sequence: the first element is the operator, the rest are arguments:

```
(op arg1 arg2 ...)
```

Expressions evaluate inside-out:

```lisp
(+ 1 2 (* 3 4))   ; (* 3 4) -> 12, then (+ 1 2 12) -> 15
```

#### Comments

`;` starts a line comment (everything to end of line is ignored), following standard Lisp convention. `;;;;` separator lines are common style for section breaks.

#### BNF Grammar

```bnf
program  ::= expr*
expr     ::= atom
           | number
           | string
           | "'" expr
           | "(" expr* ")"
           | "[" expr* "]"
atom     ::= <UTF-8 sequence not containing ( ) [ ] " or whitespace>
number   ::= "-"? [0-9]+ ("." [0-9]+)?
string   ::= '"' <any bytes except '"'> '"'
```

Notes:
- `[expr*]` rank is determined at eval time from the shapes of the evaluated elements. Scalar elements (including a single one) produce a rank-1 vector. All equal-shaped rank-k elements produce a rank-(k+1) tensor. Plain numbers (not inside `[...]`) are rank-0 scalars.

### Primitive Operations

See [docs/primitives.md](primitives.md) for the full reference. The sections below describe the major categories and any design constraints worth noting.

#### Tensor Constructors and Inspection

| Primitive             | Description                                                              |
|-----------------------|--------------------------------------------------------------------------|
| `(zero n)`            | Rank-1 zero tensor of length n                                           |
| `(make-tensor n v)`   | Rank-1 tensor of n elements filled with value v                          |
| `(shape t)`           | Returns the shape as a vector, e.g. `[2 3]`                              |
| `(rank t)`            | Returns the number of dimensions. Returns 0 for plain numbers (scalars). |
| `(slice t i)`         | Element or sub-tensor at index `i` along axis 0                          |
| `(slice-range t s e)` | Sub-tensor of rows `[s, e)` along axis 0                                 |
| `(col-slice M i)`     | Extract column `i` as a rank-1 vector (used for embedding lookup)        |
| `(head t)`            | First element along axis 0                                               |
| `(tail t)`            | All elements after the first                                             |
| `(reshape t shape)`   | Change shape without moving data; new shape given as a tensor            |
| `(vstack A B)`        | Stack two tensors row-wise; rank-1 inputs treated as single rows         |
| `(tensor? x)`         | Returns `#t` if x is a tensor                                            |

#### Arithmetic Primitives

Arithmetic operates element-wise on tensors with scalar broadcast:

```lisp
(+ [[1 2] [3 4]] 1)               ; broadcast: adds 1 to every element
(* [[1 2] [3 4]] [[5 6] [7 8]])   ; element-wise multiply
```

Matrix multiplication and transpose are distinct primitives:

```lisp
(@ A B)         ; matrix multiply (alias for matmul)
(matmul A B)
(T A)           ; transpose (alias for transpose)
```

#### Vector Math

Element-wise and reduction operations over tensors:

```lisp
(dot [1 2 3] [4 5 6])       ; => 32
(normalize [3 4])           ; => [0.6 0.8]
(sum [1 2 3 4])             ; => 10
(argmax [0.1 0.7 0.2])      ; => 1
(softmax logits)            ; numerically stable softmax
(layer-norm x eps)          ; subtract mean, divide by std
```

See [docs/primitives.md](primitives.md) for the full reference.

#### Symbolic

`match` performs one-directional unification. Pattern variables are prefixed with `?`. Returns an association list of bindings or `ERR` on failure:

```lisp
(match '(?x is ?y) '(sky is blue))
; => ((x . sky) (y . blue))
```

#### Mutation

`define` creates a new binding in the global environment. Calling it a second time with the same name does not update the existing binding; it prepends a shadow binding that hides the old one. This is fine for one-off definitions but causes unbounded environment growth in loops.

`setq` is the explicit mutation primitive. It walks the environment, finds the first binding for the given name, and updates its value in place. It returns the new value and errors if the name is not already bound.

```lisp
(define x 1)
(setq x 42)     ; x is now 42, env is unchanged in length
x               ; => 42

(setq y 1)      ; => ERR, y was not defined
```

The convention is: use `define` for the initial binding, use `setq` for all subsequent updates. This keeps intent explicit and prevents silent environment growth in training loops or iteration.

**`setq` with lists:** `setq` performs an in-place pointer update. Assigning a list value (e.g. a cons chain) can confuse the GC because the old cons cells stay live. Use `define` for list variables and reserve `setq` for scalars and tensors.

#### Kept from tinylisp

```
eval, quote, if, cond, let*, lambda, define, and, or, not, equal, <, >
```

These work unchanged. `cons` / `car` / `cdr` remain available but only operate on cons cells (s-expressions), not tensors.

**`let*` single-body constraint:** each `let*` form takes exactly one body expression. To sequence multiple effects (print, then gc, then recurse), chain nested `let*` forms with dummy bindings:

```lisp
(let* (result (compute x))
(let* (_ (print result))
(let* (_ (gc))
(next-step result))))
```

**Empty list is `()`:** the atom `nil` is not pre-bound in the environment — evaluating it returns `ERR`. Always use `()` for the empty list in base cases.

#### I/O and Files

```lisp
(print x)               ; print x followed by newline; returns x
(load "file.lisp")      ; evaluate a Lisp file
```

#### GGUF Model Weights

Basis can load pretrained model weights directly from GGUF files (the format used by llama.cpp). Each tensor in the file is interned as a global binding named after its GGUF key:

```lisp
(load-gguf "models/gpt2.Q4_0.gguf")
; token_embd.weight, blk.0.attn_qkv.weight, ... are now bound
(shape token_embd.weight)   ; => [50257 768]
```

BPE tokenization for GPT-2 style models is also available:

```lisp
(load-gguf-vocab "models/gpt2.Q4_0.gguf")
(tokenize "once upon a time")       ; => [27078 2402 257 640]
(detokenize [27078 2402 257 640])   ; => "once upon a time"
(token->str 15496)                  ; => "Hello"
```

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

See also:
- [XOR neural network example](../test_data/xor_net.lisp)
- [GPT-2 tokenizer demo](../test_data/test_tokenizer.lisp)
- [GPT-2 autoregressive generation](../test_data/gpt2_generate.lisp)


## 4. Appendices and References


#### 4.1 Definitions and Abbreviations

| Term          | Definition                                                                      |
|---------------|---------------------------------------------------------------------------------|
| NaN-boxing    | Encoding type tags and ordinals inside the payload bits of IEEE 754 NaN doubles |
| Ordinal       | The low 32 bits of a NaN-boxed value; used as a heap or table index             |
| Homoiconicity | Property where code and data share the same representation                      |
| Rank          | The number of dimensions of a tensor (scalar=0, vector=1, matrix=2, ...)        |
| Shape         | A vector of the sizes along each dimension, e.g. `[2 3]` for a 2×3 matrix       |
| REPL          | Read-Eval-Print Loop — interactive interpreter mode                             |
| `hp`          | `lisp_state_t` field — heap pointer, top of the atom string heap (grows upward) |
| `sp`          | `lisp_state_t` field — stack pointer, top of the cons cell stack (grows down from N) |
| `th`          | `lisp_state_t` field — tensor heap pointer, next free slot in `tensor_heap[]`   |
| `env`         | `lisp_state_t` field — global environment, a linked list of `(name . value)` pairs |
| Primitive     | A built-in function implemented in C and registered in the `prim[]` table       |

#### 4.2 References

- **tinylisp** — Robert A. van Engelen (2022). Original NaN-boxing Lisp in ~200 lines of C. The core eval/apply/gc machinery Basis is built on.
- **r2_maths.h** — vendored single-header math library providing `vec2`, `vec4`, and `vecn_*` / `mat_*` operations over flat `float*` arrays.
- **r2_strings.h** — vendored single-header UTF-8 string library providing `utf8_len`, `str_to_utf8`, and the `s8`/`rune` types.
- **IEEE 754** — the floating-point standard that makes NaN-boxing possible: quiet NaN values with arbitrary payloads in bits 0–50.

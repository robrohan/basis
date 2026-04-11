# 𝔹asis

A Lisp interpreter where tensors are the fundamental data type. Vectors, matrices, and N-dimensional arrays are first-class values. Code and data share the same tensor structure, in an attempt to preserve Lisp's homoiconicity.

## The idea

In standard Lisp, everything is a list. In Basis, most everything is a tensor. A scalar is a rank-0 tensor, a vector rank-1, a matrix rank-2, and so on. The `[...]` literal syntax constructs tensors directly, and sub-expressions inside a tensor are evaluated at runtime, so you can write things like:

```lisp
(define x 3)

; tensor elements can be any expression
[(+ 3 x) x]
; => [6 3]

; build a matrix with computed values
[[(* x x) 0]
 [0       (* x x)]]
; => [[9 0] [0 9]]

; tensors are code too — store unevaluated, eval later
(define template '[(+ x 1) x])
(define x 10)
(eval template)
; => [11 10]
```

## Build

```sh
make fetch         # download 3rd party libraries (all from the same author)
make build         # build the interpreter (for testing)
make release_cli   # build the release runtime
make test          # run unit tests
```

### Linux — BLAS acceleration

On Linux the build auto-detects OpenBLAS via `pkg-config`. Install it first or
the build will silently fall back to the unaccelerated path:

```sh
sudo apt-get install libopenblas-dev   # Ubuntu / Debian
make build
```

Run a file:

```sh
./build/Darwin/arm64/basis -f myfile.lisp
```

Start the REPL:

```sh
./build/Darwin/arm64/basis
```

## Examples

```lisp
; dot product
(dot [1 2 3] [4 5 6])
; => 32

; matrix multiply
(@ [[1 2] [3 4]] [[5 6] [7 8]])
; => [[19 22] [43 50]]

; chain: A * Aᵀ
(define A [[1 2] [3 4]])
(@ A (T A))
; => [[5 11] [11 25]]

; scale a vector with a lambda
(define scale (lambda (v s) (* v s)))
(scale [1 2 3] 10)
; => [10 20 30]

; build a rotation-like matrix from a variable
(define t 1.5)
[[(+ t 0) 0]
 [0       t]]
; => [[1.5 0] [0 1.5]]

; unicode atoms work fine
(define 🥧 3.14159)
(* 🥧 2)
; => 6.28318

; set! updates a binding in place (use define once, set! after)
(define W [0.1 0.2 0.3])
(set! W (+ W 1))
W
; => [1.1 1.2 1.3]
```

## Documentation

- [Language design](docs/design.md) — syntax, BNF, evaluation model
- [Primitives reference](docs/primitives.md) — full list of built-in functions

## Example files

| File | Description |
|---|---|
| [test_data/cust_xor_net.lisp](test_data/cust_xor_net.lisp) | 2-layer XOR network: ReLU hidden layer, sigmoid output, backprop weight updates with `set!` |
| [test_data/cust_projection.lisp](test_data/cust_projection.lisp) | 3D camera matrix demo: view matrix, perspective projection, live s-expression matrix entries |
| [test_data/cust_matrix.lisp](test_data/cust_matrix.lisp) | Basic matrix construction and arithmetic scratch pad |
| [test_data/mod_kb.lisp](test_data/mod_kb.lisp) | Knowledge base library: assert/retract/query facts using `match` |
| [test_data/mod_transformer.lisp](test_data/mod_transformer.lisp) | Transformer building blocks: attention, feed-forward, layer norm, GeLU |
| [test_data/gpt2_generate.lisp](test_data/gpt2_generate.lisp) | Autoregressive text generation with GPT-2 weights loaded from a GGUF file |
| [test_data/test_tokenizer.lisp](test_data/test_tokenizer.lisp) | GPT-2 BPE tokenizer demo: `tokenize`, `detokenize`, `token->str` |

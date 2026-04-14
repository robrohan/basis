---
Project: Basis
Date: 2026-04-15
---

# JSON Interface Design

## Library

- https://github.com/json-parser/json-parser
- Single-header style C library, no dependencies, public domain
- Returns a `json_value*` tree that is walked to build Lisp structures

---

## 1. JSON → Lisp  (`json-parse`)

### Type Mapping

| JSON            | Basis Lisp                     |
|-----------------|--------------------------------|
| `object {}`     | alist `((key . val) ...)`      |
| `array []`      | list `(val val ...)`           |
| `string`        | STR `"hello"`                  |
| `number`        | double                         |
| `true`          | `#t`                           |
| `false`         | `()`                           |
| `null`          | `()`                           |

Object keys are interned as atoms (not strings) so `assoc` works with
quoted symbols directly. Key strings that are not valid Lisp atom names
(e.g. contain spaces or hyphens) are interned as-is — the atom heap
already handles arbitrary byte sequences.

### Example

```json
{
  "name": "dog",
  "mass": 30,
  "tags": ["animal", "pet"],
  "bbox": [1.0, 2.0, 3.0],
  "owner": { "name": "Alice", "age": 42 },
  "alive": true,
  "nickname": null
}
```

Becomes:

```lisp
((name . "dog")
 (mass . 30)
 (tags . ("animal" "pet"))
 (bbox . (1.0 2.0 3.0))
 (owner . ((name . "Alice") (age . 42)))
 (alive . #t)
 (nickname . ()))
```

### Accessing Values

Basis has cons cells and dotted pair syntax (`(a . b)` works at read time)
but `assoc` is not exposed as a user-callable primitive — the C internal
`assoc` is wired to environment lookup only. The user-facing `assoc` lives
in `mod_kb.lisp` and must be loaded first:

```lisp
(load "test_data/mod_kb.lisp")
(define data (json-parse s))

(assoc 'name data)                    ; => "dog"
(assoc 'mass data)                    ; => 30
(assoc 'name (assoc 'owner data))     ; => "Alice"
```

**Open question:** expose C `assoc` as a primitive, or make it part of a
standard library that is always loaded? See stdlib layout discussion ticket.

### Loading into the KB

alists drop straight into `kb-assert` since KB facts share the same structure:

```lisp
(define kb (kb-assert data kb))
(kb-query '(?x name ?y) kb)
```

### Numeric Arrays and Tensors

JSON arrays of all numbers (`[1.0, 2.0, 3.0]`) are parsed as lists by
default, not tensors. This avoids silent failure on mixed arrays like
`[1, "a", true]`. The caller promotes explicitly when needed:

```lisp
(define bbox (make-tensor (assoc 'bbox data)))
```

This is intentional — `json-parse` should be predictable, not clever.

---

## 2. Lisp → JSON  (`json-stringify`)

### Type Mapping

| Basis Lisp                       | JSON             | Detection rule                                      |
|----------------------------------|------------------|-----------------------------------------------------|
| alist `((key . val) ...)`        | `object {}`      | first element is a cons pair whose `car` is an atom |
| list `(val val ...)`             | `array []`       | first element is not a cons pair with atom car      |
| STR `"hello"`                    | `string`         | STR tag                                             |
| double                           | `number`         | number type                                         |
| `#t`                             | `true`           | l_tru                                               |
| `()`                             | `null`           | NIL                                                 |
| tensor rank-1 `[1.0 2.0]`       | `[1.0, 2.0]`    | TENS tag, rank 1                                    |
| tensor rank-2+ `[[...][...]]`   | nested `array`   | TENS tag, rank > 1                                  |
| atom (bare symbol)               | `"symbol"`       | ATOM tag — emitted as a quoted string               |

### Alist Detection Heuristic

The distinction between an alist (object) and a list of lists (array) is:

```
if (T(car(x)) == CONS && T(car(car(x))) == ATOM)  → object
else                                                → array
```

This matches Common Lisp convention. A list of non-pair elements or pairs
with non-atom keys is always treated as an array.

### Example

```lisp
(json-stringify
    '((name . "Alice")
      (scores . (98 87 76))
      (active . #t)
      (address . ())))
```

Produces:

```json
{"name":"Alice","scores":[98,87,76],"active":true,"address":null}
```

### Tensors

Rank-1 tensors emit as flat JSON arrays. Higher-rank tensors emit as
nested arrays, row by row:

```lisp
(json-stringify [1.0 2.0 3.0])
; => "[1.0,2.0,3.0]"

(json-stringify (make-tensor '(2 2) '(1.0 2.0 3.0 4.0)))
; => "[[1.0,2.0],[3.0,4.0]]"
```

### Round-trip

JSON → Lisp → JSON is not guaranteed to be byte-identical (key order,
float formatting) but is semantically stable:

```lisp
(equal (json-parse (json-stringify data)) data)  ; #t for well-formed data
```

---

## 3. Primitives Summary

| Primitive               | Signature                    | Notes                          |
|-------------------------|------------------------------|--------------------------------|
| `json-parse`            | `(json-parse str)` → value   | str is a STR or ATOM           |
| `json-stringify`        | `(json-stringify val)` → STR | inverse of json-parse          |

Both live in a new `src/json_support.c` alongside the other extension
files (`tinysymbolic.c`, `gguf_loader.c`, etc.).

---

## 4. Error Handling

- Malformed JSON → returns `ERR`
- Unsupported types (closures, primitives) in `json-stringify` → returns `ERR`
- Errors are consistent with how `match` and other primitives signal failure

---

## 5. Implementation Notes

- json-parser's `json_value*` tree is fully built before walking begins —
  no streaming; fine for the sizes Basis targets
- The C wrapper recursively walks `json_value*` and calls `atom()`, `cons()`,
  `box()` etc. to build the Lisp heap — same pattern as the GGUF loader
- `json-stringify` is implemented in C for performance but could be prototyped
  in Lisp first using `mod_kb.lisp` primitives as a reference

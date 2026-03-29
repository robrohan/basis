---

kanban-plugin: board

---

## 🧊 Backlog

- [ ] Add regression tests to test_main.c for multi-byte atoms used as values

- [ ] Wire r2_maths.h for vec2/vec4/mat3/mat4 fast paths on common tensor shapes
- [ ] Implement matmul primitive using r2_maths mat_mul for general case
- [ ] Implement scalar broadcast in arithmetic (scalar op tensor)
- [ ] Implement head/tail sugar: head = slice 0, tail = slice 1:end
- [ ] Update README with syntax overview and examples

## 📝 Todo

- [ ] Update gc: trace and collect tensors on the tensor heap
- [ ] Write integration tests for tensor literals, shape, rank, slice
- [ ] Write integration tests for arithmetic and matmul on tensors

## 👨‍💻 Doing (0)

## ✅ Done

**Complete**
- [x] Write design document (docs/design.md) covering syntax, BNF, primitives, and r2_maths.h mapping
- [x] Replace #define I unsigned with typedef uint32_t I using <stdint.h>
- [x] Replace #define L double with typedef double L with NaN-boxing explanation comment
- [x] Replace unsigned long long casts with uint64_t throughout NaN-boxing functions
- [x] Make NaN tag globals const (ATOM, PRIM, CONS, CLOS, NIL)
- [x] Rename not() to is_nil() to avoid <iso646.h> macro conflict introduced by <stdint.h>
- [x] Fix char see to int see so EOF (-1) is handled correctly on all platforms
- [x] Fix get() return type from char to int for consistency with see
- [x] Fix seeing() parameter from char to int
- [x] Include r2_strings.h and add R2_STRINGS_IMPLEMENTATION to one .c file
- [x] Update scanner: use utf8_len() to detect multi-byte sequences and collect all bytes before token boundary checks
- [x] Update atom buffer: ensure buf[] is large enough for multi-byte atom names (emoji = 4 bytes each)
- [x] Update atom interning: verify strcmp/strlen still work correctly on raw UTF-8 bytes in the heap
- [x] Write tests: define and call lambdas with emoji names (e.g. 🔥, λ, π)
- [x] Write C unit tests (src/test_main.c) using r2_unit.h covering NaN-boxing, atoms, cons/car/cdr, eval, closures, recursion, UTF-8
- [x] Add make test target to Makefile
- [x] Fix REPL prompt flushing (fflush stdout) so multi-byte input works correctly in interactive mode
- [x] Add TENS NaN-box tag (0x7ffd) to tinylisp.h alongside ATOM, PRIM, CONS, CLOS, NIL
- [x] Define tensor_t struct (rank, shape[8], len, float* data) and tensor heap in tinylisp.h
- [x] Update scanner: add [ and ] as token delimiters; treat , as whitespace
- [x] Update parser: parse tensor literals [ elem ... ] into TENS values
- [x] Update parser: handle (op, [args]) expression form
- [x] Update eval: TENS values are self-evaluating (like numbers)
- [x] Update print/printlist: display TENS values as [ e1 e2 ... ]
- [x] Implement shape primitive: (shape, [t]) → vector of dimensions
- [x] Implement rank primitive: (rank, [t]) → scalar
- [x] Implement slice primitive: (slice, [t, i]) → element or sub-tensor at index i
- [x] Add tensor? predicate primitive
- [x] Update arithmetic primitives (+, -, *, /) to operate element-wise on TENS
- [x] Fix scan() do-while to stop at [ and ] token boundaries

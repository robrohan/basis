---

kanban-plugin: board

---

## 🧊 Backlog

- [ ] car and cdr don't seem to work with tensors
- [ ] eq? does not work with tensors (element wise?)
- [ ] Format output of a matrix in a way that is easier to read, but still conforms to homoiconicity
- [ ] add maths functions sinf, cosf, etc
- [ ] add `include`, `require` or `import` to parse other files and use functions etc in those files
- [ ] (pow (slice M 0)) returns [1 nan nan nan] - note missing second parameter. Should be error
- [ ] How to make a service

## 📝 Todo

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
- [x] Update scanner: add [ and ] as token delimiters
- [x] Update parser: parse tensor literals [ elem ... ] into TENS values
- [x] Update eval: TENS values are self-evaluating (like numbers)
- [x] Update print/printlist: display TENS values as [ e1 e2 ... ] with nested rows for rank>1
- [x] Implement shape primitive
- [x] Implement rank primitive
- [x] Implement slice primitive
- [x] Add tensor? predicate primitive
- [x] Update arithmetic primitives (+, -, *, /) to operate element-wise on TENS
- [x] Fix scan() do-while to stop at [ and ] token boundaries
- [x] Update gc: mark/compact/patch tensor heap after each REPL cycle
- [x] Write integration tests for tensor literals, shape, rank, slice, arithmetic, matmul (51 tests)
- [x] Implement matmul primitive using r2_maths mat_mul (mat*mat, mat*vec) with @ alias
- [x] Implement scalar broadcast in arithmetic (scalar op tensor)
- [x] Add transpose primitive with T alias using r2_maths mat_transpose
- [x] Add vec= predicate, dot, length, length2, dist, dist2, normalize, abs, sqrt, pow, zero primitives via vecn_*
- [x] Update README with syntax overview, build instructions, and full primitives table
- [x] Add tests for all vecn primitives (62 tests total)
- [x] Wire vec2/vec4 fast paths in dot, length, length2, dist, dist2, normalize, abs, sqrt, pow
- [x] Implement head/tail sugar primitives
- [x] Add regression tests for multi-byte atoms used as values (67 tests total)
- [x] Fix tensor literal parser to use parse() so s-expressions work as elements: [(+ 3 x) x]
- [x] Add make-tensor primitive as runtime backend for [ ] literals
- [x] Add gc primitive so scripts can force garbage collection
- [x] Verify homoiconicity: quoted tensor literals store as code, eval triggers evaluation
- [x] Fix REPL/file-mode print bug where last tensor result was lost after gc()

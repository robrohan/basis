---

kanban-plugin: board
legend:
- 🪲 Bug with current state
- 📢 Feature to add soon
- 🔮 Future Feature (more discussion)

---

## 🧊 Backlog

- [ ] 📢 Build a small C tool (name and location TBD) that reads a `.lisp` file byte by byte and writes a C header containing a `static const unsigned char bundle_code[]` byte array plus its length — no string escaping, no external tools, just a `fgetc` loop emitting decimal values; add a Makefile target to compile the tool itself, and a `bundle` target that runs it then recompiles the interpreter with `-DHAVE_BUNDLE` to produce a self-contained binary named after the input file
- [ ] 📢 Bundle runtime flags: the compiled binary always runs its embedded lisp first; add `-r` to drop into the REPL after (for live debugging / hot-patching), and keep `-f x.lisp` to load an additional file into the same env on top of the bundle (can redefine symbols, add helpers); flags compose — `./myapp -f patch.lisp -r` loads bundle → patch → REPL
- [ ] 🔮 Proof engine (`test_data/mod_proof.lisp`): backward-chaining prover built on `unify`; rewrite rules stored as plain lists `(rule lhs rhs)`; `apply-subst`, `rewrite-once`, and `prove` implemented in Lisp; supports mathematical induction via base-case axioms + inductive-step rules; see `docs/proof_engine.md` for full design; depends on `unify` C primitive — implement that first (already in backlog)
- [ ] 🔮 Add stb_image.h (https://github.com/nothings/stb/blob/master/stb_image.h) as a vendor header and expose a `(load-image "path")` primitive that decodes PNG/JPG/etc into a rank-3 tensor [H W C] of floats normalised 0–1 — foundation for building CNNs in Lisp
- [ ] 📢 BLOCKER (Squeeze integration): Add (read) primitive — reads and parses one complete S-expression from the current input stream (stdin in file/REPL mode), blocks until a full expression is available, returns () on EOF; this is standard Common Lisp (read); required for the co-process pattern where basis starts once at startup, loads GGUF models into memory, then processes a stream of requests from a host process (Go proxy) without ever restarting — without (read) every request requires a new fork+exec which reloads all models from disk
- [ ] 🪲 Scanner string literal hard limit of 253 bytes silently truncates longer strings — scan buf[] is fixed at 254 chars; increase to at least 2048 or replace with dynamic allocation; affects proxy/CGI use cases where URLs and JSON bodies easily exceed 253 bytes
- [ ] 📢 Add (read-line) primitive: reads one line from stdin (or current input-stream) as a string, returns () on EOF — lower-level companion to (read); useful when the host sends newline-delimited text rather than S-expressions
- [ ] 📢 Add string primitives: (string-length s) using s8.len for rune count, (string-ref s i) for rune at index, (string-append s1 s2) — use S()/free_S() around operations that need rune-aware counting, raw bytes for storage
- [ ] 📢 add https://github.com/json-parser/json-parser json reading support; interface design in `docs/json_interface.md` — `(json-parse str)` → alist with atom keys, `(json-stringify val)` → STR; alists become objects, lists become arrays, tensors become nested arrays; numeric arrays stay as lists by default (caller promotes to tensor explicitly)
- [ ] 🔮 Integrate seaweb (robrohan's C HTTP server) as a vendor library and expose HTTP primitives — `(http-listen port handler)` or similar — so simple services can be written in Basis Lisp; depends on string primitives and (read-line)/(read) being solid first
- [ ] 🔮 Discussion: standard library layout — currently `mod_kb.lisp`, `mod_math.lisp` etc. are loose files in `test_data/` that must be explicitly loaded; as primitives like `assoc`, `map`, `filter`, `append` are re-implemented in Lisp across multiple modules, decide: (a) expose more as C primitives, (b) define a `std/` directory always loaded at startup, or (c) keep explicit `load` calls but move stdlib out of `test_data/`; triggered by JSON interface needing `assoc` from `mod_kb.lisp`
- [ ] 🔮 Replace gc_core() bump-reset with proper mark-sweep: mark phase traverses all values reachable from env (and explicit GC roots), sweep/compact phase slides live cons cells down and fixes up all CONS/CLOS ordinals via a forwarding table — enables safe gc() calls mid-computation and fixes kb-infer exhaustion on large KBs
- [ ] 🪲 tinylisp has no TCO and evlis/eval/apply are deeply mutually recursive — GPT-2 forward pass exhausts the default C stack; fix by adding tail-call trampolining or iterative eval, or at minimum detect stack depth and error gracefully instead of SIGBUS
- [ ] 📢 Add (princ x) primitive following Common Lisp conventions: human-readable output without escapes (for strings, print bytes without surrounding quotes)
- [ ] 📢 Add (defun name (args) "docstring" body) following Emacs Lisp convention: sugar for (define name (lambda (args) body)) with the docstring stored on the binding so (doc name) can retrieve it; depends on string literals being implemented first
- [ ] 📢 Better error messages somehow 
- [ ] 🪲 car and cdr don't seem to work with tensors - first and rest do, maybe just call those if someone passes a tensor to car or cdr?
- [ ] 🪲 (pow (slice M 0)) returns [1 nan nan nan] - note missing second parameter. Should be error (needs better error handling)
- [ ] 📢 KB tensor properties: storing tensors in KB facts requires a helper like (fact s p o) since quote prevents tensor literal evaluation — (fact 'car 'bbox [2.0 1.5 4.5]) works but is ugly; proper fix is a (triple s p o) builtin or quasiquote/unquote support
- [ ] 📢 Add (unify p1 p2) for bidirectional pattern matching: finds substitutions that make two terms equal, both terms can contain variables; needed for theorem proving and constraint solving beyond one-directional match
- [ ] 📢 Neurosymbolic demo: knowledge base with physical properties (mass, bounding box as tensors) alongside is-a/has-a facts; query KB for object properties, combine with tensor ops for similarity
- [ ] 🔮 Proxy server with rules engine (see ./docs/idea_server.md)
- [ ] 🔮 Very simple physics world model: mass ratio crush check, bounding sphere overlap, a handful of rules (crush/bounce/slide); designed to answer LLM-posed questions like "what happens if X hits Y"; depends on KB and match being solid first
- [ ] 🔮 Batched training: parallelize train-epoch across CPU cores — each example is independent, spawn N threads each running train-one on a subset, accumulate gradients, single weight update step; revisit if project moves beyond research POC
- [ ] 🔮 cuBLAS support: add HAVE_CUBLAS build path in tinytensor.c alongside existing HAVE_BLAS (OpenBLAS) paths so GPU-accelerated mat_mul can be used on Linux machines with CUDA
- [ ] 🔮 Logistic regression with explicit regularization: implement in Lisp with no abstracted string parameters — user writes the loss function directly (MSE + λ*Σw² for L2, MSE + λ*Σ|w| for L1), sets λ as a numeric variable, and sees the math working rather than passing penalty="l2" and C=0.01 to a black box; good teaching example for the KB / neurosymbolic direction


## 📝 Todo

## 👨‍💻 Doing (0)

## ✅ Done

**Complete**
- [x] Add regex support via UTF-8-patched tiny-regex-c: `(re-match pattern text)` returns `(start len)` byte offsets or `()`; `\w`/`\d`/`\s` ASCII-only, `.` and `[α-ω]` ranges are codepoint-aware; vendored as `vendor/re.c`
- [x] Add `(substring text start len)` primitive: extracts a byte slice from a string; composes with `re-match` to extract matched text
- [x] REPL slash commands: `/quit` exits cleanly, `/?` shows help screen with key bindings and all registered commands; extensible via `register_cmd()` in new `src/cmd.c`
- [x] Refactor `main.c` execution pipeline: explicit phase sequence init→load→REPL→done; add `-r` flag to force REPL after file load; add `run_string()` via `fmemopen` as shared in-memory eval primitive for future bundle support
- [x] Refactor all interpreter globals into lisp_state_t context struct: interpreter is now multi-instance capable (Step 1 of proxy server design)
- [x] REPL: editline line editing (left/right cursor, backspace), up/down history, coloured stats prompt via r2_termui, multi-line continuation with _ prompt, broken out into src/repl.c
- [x] Fix multi-head attention in gpt2_generate.lisp: split Q/K/V into 12 heads of 64 dims, run attention per-head with causal mask, vstack results; add causal-mask C primitive
- [x] Fix generate loop: use global context tensor + set! so gc() can safely free forward-pass intermediates without corrupting the token history; replace (eq? n 0) with (< 0 n)
- [x] Add (match pattern data) primitive in tinysymbolic.c: ?-prefixed atoms are variables, returns bindings alist (? stripped from keys) or ERR; consistent variable binding checked; handles atoms, lists, numbers, nil
- [x] Add sin/cos primitives: scalar or element-wise on tensors; same pattern as exp; added to tinytensor.c alongside abs/sqrt
- [x] Fix eq? for tensors: moved to runtime.c (where both heaps are visible); deep equality checks rank + shape + all elements via tensor_equal(); also fixed vec= to use the same shared helper
- [x] Add 3D camera matrix demo (test_data/projection_test.lisp): view matrix from position/yaw/pitch, perspective projection, combined camera matrix, point transforms; rotation entries are live s-expressions evaluated at call time
- [x] Add string literal support ("..." syntax): scanner collects bytes between quotes, atomic() interns raw bytes in atom heap with STR NaN-box tag; self-evaluating; print displays with quotes; raw bytes sent to stdout (UTF-8 terminal renders correctly); S()/free_S() reserved for rune-aware primitives
- [x] Add (load "file.lisp") primitive: swaps input_stream FILE* pointer so nested loads and REPL resumption work without stdio buffer corruption; accepts STR or ATOM path argument
- [x] Add (set! x val) primitive for explicit mutation: updates the first matching binding in env in-place, returns val, errors if x is not already defined. Updated xor_net.lisp weight updates to use set!.
- [x] Fix GC in file mode: call gc() after each top-level expression in run_file(); also added explicit (gc) call inside xor_net.lisp train loop between epochs so tensor heap does not overflow during long training runs.
- [x] Add (print x) primitive: prints x then newline, returns x (CL convention, usable inline)
- [x] Silence file mode: stop echoing every expression result (match Common Lisp load behaviour); explicit (print x) calls are the only output — depends on print primitive being available first
- [x] Refactor main.c: extract repl() and run_file() as separate functions — the two modes are diverging enough to warrant it
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
- [x] Add `;` line comment support to scanner (standard Lisp convention; `;;;;` separator lines no longer produce ERR)
- [x] XOR neural network example converging correctly in test_data/xor_net.lisp (2-layer ReLU+sigmoid, backprop with outer product gradient)

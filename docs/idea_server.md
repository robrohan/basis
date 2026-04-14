# Basis Proxy Server — Design Document

A Go-based HTTP proxy/inspection server where per-request logic is written in
Lisp (basis), with the ability to run ML inference (GGUF models) as part of
that inspection. The C interpreter is embedded via cgo. Go owns the network
layer; C owns the Lisp + tensor runtime.

---

## Goals

- HTTP/HTTPS proxy server in Go (stdlib `net/http/httputil`)
- Per-request inspection rules written in basis Lisp
- ML inference available inside Lisp (GGUF models, tensor ops)
- Very low overhead on the fast path (pure Lisp rules, no ML)
- Correct concurrent access — multiple requests in parallel, no state corruption
- Model weights loaded once at startup, shared read-only across all instances

---

## Architecture Overview

```
[client request]
      |
      v
[Go proxy server]  — net/http, goroutine per request
      |
      |--- fast path: pure Lisp rules (microseconds, no ML)
      |         |
      |         v
      |    [lisp_state_t instance from pool]
      |         |
      |    pass / block / tag-for-ML
      |
      |--- slow path: ML inference (milliseconds, pool-limited)
      |         |
      |         v
      |    [lisp_state_t instance from pool]
      |    [universe_t* — shared read-only model weights]
      |         |
      |    pass / block / score
      |
      v
[upstream server]
```

The Go layer never touches Lisp values directly. It passes request data as
C strings and receives a result code (pass, block, score) back across the
cgo boundary.

---

## C-Side Changes

### 1. Refactor globals into `lisp_state_t`

All current globals in `tinylisp.c`, `tinytensor.c`, `runtime.c` must move
into a per-instance struct. This is the most mechanical part of the work.

Current globals to move:
```c
// tinylisp.c
L    cell[N];       // heap + stack pool
I    hp;            // heap pointer (grows up)
I    sp;            // stack pointer (grows down from N)
L    env;           // global environment alist
int  see;           // tokenizer lookahead character
FILE *input_stream; // current input (stdin or file)

// tinytensor.c
tensor_t tensor_heap[MAX_TENSORS];
I        tensor_count;

// runtime.c / primitives
L    prim[MAX_PRIMS];
I    prim_count;
```

Target struct (in a new `lisp_state.h`):

```c
#define MAX_ACTIVATIONS 0x1000  // activation tensors per instance (not weights)

typedef struct lisp_state_t {
    // Lisp heap + stack (shared pool, dual-purpose)
    L    cell[N];           // ~4MB per instance (N = 0x80000)
    I    hp;                // heap pointer
    I    sp;                // stack pointer

    // Environment
    L    env;               // local env alist (chains to universe for lookups)

    // Tokenizer state
    int  see;               // lookahead char
    FILE *input_stream;     // current input stream

    // Activation tensor heap (working memory only, NOT model weights)
    tensor_t activation_heap[MAX_ACTIVATIONS];
    I        activation_count;

    // Primitive table (initialized once per instance, then read-only)
    L    prim[MAX_PRIMS];
    I    prim_count;

    // Pointer back to shared model weights (read-only, never written)
    struct universe_t *universe;
} lisp_state_t;
```

Every function that currently uses globals (`eval`, `atom`, `gc`, `scan`,
`parse`, `apply`, etc.) gains a `lisp_state_t *s` parameter and accesses
state via `s->hp`, `s->cell`, `s->env`, etc.

This is tedious but purely mechanical. No logic changes required.

### 2. `universe_t` — shared read-only model weights

A single global instance loaded at startup before any goroutine starts.
Never written to after `universe_init()` returns. No mutex needed.

```c
#define UNIVERSE_ATOM_HEAP  0x20000   // atom interning for weight names
#define MAX_MODEL_TENSORS   0x1000    // up to 4096 weight tensors

typedef struct universe_t {
    // Atom heap for weight names ("wte.weight", "wpe.weight", etc.)
    char     atom_heap[UNIVERSE_ATOM_HEAP];
    I        atom_hp;

    // Weight tensor metadata (headers only; float data is malloc'd separately)
    tensor_t tensor_heap[MAX_MODEL_TENSORS];
    I        tensor_count;

    // Name → tensor index lookup (simple linear scan is fine at load time)
    // At runtime, (universe "name") lookups are also linear but over a
    // fixed small list — acceptable for model sizes up to a few thousand layers
} universe_t;

// Single global — initialized once at startup
universe_t *g_universe;

// Load a GGUF file into universe. Call once before serving requests.
int universe_init(const char *gguf_path);

// Look up a weight tensor by name. Returns NULL if not found.
// SAFE to call from multiple threads concurrently (read-only after init).
tensor_t *universe_lookup(const char *name);
```

The existing `gguf_loader.c` needs to be split: the GGUF parsing and
dequantization stays, but instead of writing into the global `env` and
`tensor_heap`, it writes into `universe->tensor_heap` and
`universe->atom_heap`. The `float *data` pointers inside each `tensor_t`
remain as individual `malloc`'d allocations — these are never freed for the
lifetime of the process.

### 3. New primitive: `(universe "name")`

This is the bridge between per-instance Lisp and the shared model weights.

```c
L prim_universe(L args, lisp_state_t *s) {
    // args = ("weight-name")
    if (T(car(args)) != STR) return err;

    const char *name = /* extract string from car(args) */;
    tensor_t *src = universe_lookup(name);
    if (!src) return err;

    // Allocate a slot in the instance's activation heap
    if (s->activation_count >= MAX_ACTIVATIONS) return err;
    I idx = s->activation_count++;

    // Shallow-copy the tensor header. float *data is NOT copied.
    // Both universe and instance tensor_t point to the same float array.
    // This is safe because:
    //   - universe->tensor_heap is read-only after init
    //   - no tensor operation writes back to ->data in place;
    //     results always allocate a new tensor
    s->activation_heap[idx] = *src;

    return box(TENS, idx);
}
```

After this call, the instance can use the tensor in any normal operation:
`(matmul (universe "wte.weight") embeddings)`, `(softmax ...)`, etc. The
weight data is never duplicated. Results (new tensors from ops) are
allocated in the instance's activation heap with their own `malloc`'d data.

#### Lisp-side sugar

Optionally add a reader macro or `defun` wrapper so Lisp code reads cleanly:

```lisp
; Option A: special @-prefix syntax in the tokenizer (reader macro)
(matmul @wte.weight embeddings)

; Option B: plain function, no parser changes needed
(matmul (universe "wte.weight") embeddings)

; Option C: defun alias at startup
(defun W (name) (universe name))
(matmul (W "wte.weight") embeddings)
```

Option B requires no C changes beyond the primitive itself. Option A requires
a small tokenizer change (recognize `@foo` as `(universe "foo")`).

### 4. Garbage collection changes

Current GC (`gc_core`) discards the tensor heap and resets it. With the new
design, GC must:

1. Only reset `s->activation_count` (never free universe tensors)
2. NOT call `free()` on tensor `->data` for any tensor whose `->data`
   pointer matches a pointer in `universe->tensor_heap` — those are owned
   by the universe and shared

Simplest implementation: mark universe-origin tensors with a flag:

```c
typedef struct {
    I     rank;
    I     shape[MAX_RANK];
    I     len;
    float *data;
    int   universe_owned;   // 1 = do not free, 0 = instance owns this data
} tensor_t;
```

GC skips `free(t->data)` when `universe_owned == 1`. `prim_universe` sets
this flag when it shallow-copies the header.

### 5. Instance initialization and reset

Each pool instance needs:

```c
// Full initialization (called once when pool is created)
lisp_state_t *lisp_state_new(universe_t *u);

// Fast reset between requests (does NOT re-init primitives or universe ptr)
void lisp_state_reset(lisp_state_t *s);
```

`lisp_state_reset` runs the GC, resets `hp`, `sp`, `activation_count`, and
clears the local `env` back to the primitive table baseline. It is the
equivalent of the current REPL's per-iteration GC call. This must be fast
— it runs on every request before the instance returns to the pool.

---

## Go-Side Design

### Project layout

```
basis-proxy/
  main.go           — startup, flag parsing, pool init, server start
  proxy.go          — net/http/httputil reverse proxy, request routing
  inspector.go      — cgo bindings, pool management, inspection API
  pool.go           — lisp_state_t pool (channel-based)
  lisp/
    basis.h         — cgo-exported C header (manually written)
    (symlink or copy of basis src + vendor)
  go.mod
```

### cgo binding

`inspector.go` is the only file that imports cgo:

```go
package main

/*
#cgo CFLAGS: -O3 -march=native -fopenmp -I../src -I../vendor
#cgo LDFLAGS: -lm -fopenmp
#include "basis.h"
*/
import "C"
import "unsafe"
```

`basis.h` exposes only what Go needs:

```c
// basis.h — cgo interface (C-facing, minimal surface area)

typedef struct lisp_state_t lisp_state_t;
typedef struct universe_t   universe_t;

// Universe (model weights) — call once at startup
universe_t *universe_new(void);
int         universe_load_gguf(universe_t *u, const char *path);

// Instance pool management
lisp_state_t *lisp_state_new(universe_t *u);
void          lisp_state_reset(lisp_state_t *s);
void          lisp_state_free(lisp_state_t *s);

// Load a Lisp rules file into an instance (call after new, before pool use)
int lisp_load_file(lisp_state_t *s, const char *path);

// Evaluate a Lisp expression string. Returns result as a C string.
// Caller must NOT free the result — it points into s->cell.
// Result is invalidated by the next lisp_eval or lisp_state_reset call.
const char *lisp_eval_str(lisp_state_t *s, const char *expr);

// High-level inspection call. Builds and evaluates:
//   (inspect-request method path headers body)
// Returns: 0 = pass, 1 = block, 2 = needs-ml-inspection
int lisp_inspect_request(lisp_state_t *s,
                         const char *method,
                         const char *path,
                         const char *headers_json,
                         const char *body,
                         int         body_len);
```

### Pool (pool.go)

```go
type Pool struct {
    ch chan *C.lisp_state_t
}

func NewPool(size int, universe *C.universe_t, rulesFile string) *Pool {
    ch := make(chan *C.lisp_state_t, size)
    for i := 0; i < size; i++ {
        s := C.lisp_state_new(universe)
        if rulesFile != "" {
            cpath := C.CString(rulesFile)
            C.lisp_load_file(s, cpath)
            C.free(unsafe.Pointer(cpath))
        }
        ch <- s
    }
    return &Pool{ch: ch}
}

func (p *Pool) Acquire() *C.lisp_state_t {
    return <-p.ch   // blocks if all instances are busy
}

func (p *Pool) Release(s *C.lisp_state_t) {
    C.lisp_state_reset(s)
    p.ch <- s
}
```

Pool size should be tuned to available RAM:

```
pool_size = floor(available_ram / per_instance_ram)
per_instance_ram ≈ 4MB (cell array) + activation tensors + overhead
```

For a system with 4GB free and no model loaded: ~800 instances possible.
With a 500MB GGUF model loaded (weights shared): same 800 instances, model
memory is paid once.

### Proxy and inspection (proxy.go, inspector.go)

```go
func (srv *Server) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    s := srv.fastPool.Acquire()
    result := inspect(s, r)
    srv.fastPool.Release(s)

    switch result {
    case ResultBlock:
        http.Error(w, "blocked", http.StatusForbidden)
    case ResultNeedsML:
        // Hand off to ML pool (may block waiting for an instance)
        s2 := srv.mlPool.Acquire()
        result2 := inspectML(s2, r)
        srv.mlPool.Release(s2)
        if result2 == ResultBlock {
            http.Error(w, "blocked", http.StatusForbidden)
            return
        }
        fallthrough
    case ResultPass:
        srv.proxy.ServeHTTP(w, r)
    }
}
```

Two pools:
- `fastPool` — many instances (hundreds), no GGUF, pure Lisp rules, microseconds
- `mlPool` — few instances (sized to RAM), GGUF loaded, milliseconds

The fast pool instances are initialized without calling `universe_load_gguf`.
The ML pool instances share the same `universe_t*`.

### Startup sequence (main.go)

```go
func main() {
    flags := parseFlags()  // gguf-path, rules-file, pool-size, upstream, addr

    // 1. Load GGUF weights once (blocks until complete)
    var universe *C.universe_t
    if flags.ggufPath != "" {
        universe = C.universe_new()
        cpath := C.CString(flags.ggufPath)
        n := C.universe_load_gguf(universe, cpath)
        C.free(unsafe.Pointer(cpath))
        log.Printf("loaded %d tensors from %s", n, flags.ggufPath)
    }

    // 2. Create pools (pre-warms all instances before accepting traffic)
    fastPool := NewPool(flags.fastPoolSize, nil, flags.rulesFile)
    mlPool   := NewPool(flags.mlPoolSize, universe, flags.rulesFile)

    // 3. Build reverse proxy
    target, _ := url.Parse(flags.upstream)
    rp := httputil.NewSingleHostReverseProxy(target)

    // 4. Serve
    srv := &Server{fastPool: fastPool, mlPool: mlPool, proxy: rp}
    log.Fatal(http.ListenAndServe(flags.addr, srv))
}
```

---

## Lisp Rules Interface

Rules are loaded from a `.lisp` file at startup (once per instance, then
the env is part of the reset baseline). The proxy calls a known entry point:

```lisp
; rules.lisp — loaded into every instance at startup

; Fast path: called on every request
; Returns: 'pass, 'block, or 'inspect
(defun inspect-request (method path headers body)
  (cond
    ((blocked-path? path)   'block)
    ((suspicious? headers)  'inspect)   ; escalate to ML pool
    (t                      'pass)))

(defun blocked-path? (path)
  ; pure symbolic rules — fast, no ML
  (or (equal path "/admin")
      (equal path "/.env")))

(defun suspicious? (headers)
  ; heuristic rules before committing ML resources
  nil)

; ML path: called only when fast path returns 'inspect
; Has access to (universe "weight-name") for GGUF tensors
(defun inspect-request-ml (method path headers body)
  (let* ((emb  (embed body))
         (logits (matmul (universe "classifier.weight") emb))
         (probs  (softmax logits))
         (label  (argmax probs)))
    (if (= label 1) 'block 'pass)))
```

The C function `lisp_inspect_request` calls `(inspect-request ...)` and
maps the returned symbol to the integer result code.

---

## Thread Safety Guarantees

| Resource | Access Pattern | Safety Mechanism |
|---|---|---|
| `universe_t` | Read-only after `universe_load_gguf` | None needed |
| `universe->tensor_heap[i].data` | Read-only float arrays | None needed |
| `lisp_state_t` | One instance per goroutine at a time | Channel pool (only one goroutine holds an instance) |
| `activation_heap[i].data` | Instance-local malloc | Instance ownership |
| `g_universe` global pointer | Set once at startup | None needed after init |

No mutexes required in the hot path. The channel pool is the only
synchronization primitive.

---

## Performance Expectations

### Fast path (pure Lisp rules)
- cgo call overhead: ~100ns
- Lisp eval for simple rules: ~1–10µs
- Pool acquire/release: ~100ns (channel op)
- **Total per request: ~10–50µs** — well under 1ms

### Slow path (ML inference)
- Dominated entirely by model forward pass
- Small quantized model (Q4_K, ~100M params): ~10–50ms
- GPT-2 small (117M, F32): ~100–500ms depending on hardware
- **Throughput = pool_size / inference_latency**
  - 8 instances × 50ms inference = ~160 req/s on slow path

### Memory per instance
- `cell[N]` where N=0x80000: 524,288 × 8 bytes = **4MB**
- `activation_heap[MAX_ACTIVATIONS]`: 4096 × ~48 bytes headers + float data
- Float data per activation tensor: depends on model, typically ~1–50MB working set
- **Rule of thumb: 8–16MB per instance** for activation overhead

### GGUF model weight memory
- Paid once regardless of pool size
- Q4_K GPT-2 small: ~60MB
- Q4_K LLaMA 7B: ~4GB
- Shared via pointer — zero marginal cost per additional instance

---

## Implementation Order

1. **Refactor globals → `lisp_state_t`** (C, mechanical)
   - Move all globals into struct
   - Thread `lisp_state_t *s` through every function signature
   - Verify existing tests still pass (tests create one state, same behavior)

2. **Extract `universe_t`** (C)
   - Split `gguf_loader.c` into universe-loading vs instance-local
   - Implement `universe_init`, `universe_lookup`
   - Add `universe_owned` flag to `tensor_t`
   - Update GC to skip freeing universe-owned data

3. **Add `prim_universe`** (C)
   - Register `(universe "name")` primitive
   - Shallow-copy tensor header into instance activation heap
   - Write a Lisp-level test: load a small GGUF, call `(universe ...)`, run matmul

4. **Implement `lisp_inspect_request` C entry point** (C)
   - Accept method, path, headers, body as C strings
   - Build Lisp expression, eval, map result symbol to int
   - Handle errors gracefully (return pass on interpreter error, log it)

5. **Go proxy** (Go)
   - `pool.go`: channel-based pool
   - `inspector.go`: cgo bindings, thin wrappers
   - `proxy.go`: `ServeHTTP`, two-tier fast/ML routing
   - `main.go`: startup, flag parsing

6. **Rules file and integration test** (Lisp + Go)
   - Write `rules.lisp` with fast-path rules
   - Write a Go test that starts the proxy, sends requests, verifies block/pass

---

## Open Questions

- **Reader macro for `@name`**: worth adding to tokenizer for cleaner rules?
  Avoids string quoting: `@wte.weight` vs `(universe "wte.weight")`.

- **Response inspection**: the design covers request inspection. Inspecting
  the upstream response body (e.g., for output filtering) requires buffering
  the response, which adds latency. Use `httputil.ReverseProxy.ModifyResponse`
  but be aware of memory cost for large responses.

- **TLS termination**: `http.ListenAndServeTLS` drops in trivially if needed.
  Upstream TLS is handled by the reverse proxy automatically.

- **Hot-reload of rules**: currently rules are loaded once at pool init.
  Hot-reload would require draining the pool, resetting all instances,
  reloading. Simpler alternative: SIGHUP handler that replaces the pool.

- **Streaming bodies**: `lisp_inspect_request` receives the full body as a
  C string. For large bodies this means buffering the entire request before
  forwarding. For a proxy inspecting large uploads, consider inspecting only
  the first N bytes or using chunked inspection.

- **`input_stream` in `lisp_state_t`**: the `(load "file.lisp")` primitive
  swaps `input_stream`. This is safe per-instance but means Lisp rules cannot
  dynamically load other files during request handling without care. Restrict
  `(load ...)` to the initialization phase only, or remove it from the
  request-handling primitives.

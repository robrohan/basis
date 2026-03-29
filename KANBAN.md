---

kanban-plugin: board

---

## 🧊 Backlog

- [ ] Add TENS NaN-box tag (0x7ffd) to tinylisp.h alongside ATOM, PRIM, CONS, CLOS, NIL
- [ ] Define tensor_t struct (rank, shape[8], len, float* data) and tensor heap storage
- [ ] Update scanner: add [ and ] as token delimiters; treat , as whitespace
- [ ] Update parser: parse tensor literals [ elem ... ] into TENS values
- [ ] Update parser: handle (op, [args]) expression form
- [ ] Update eval: TENS values are self-evaluating (like numbers)
- [ ] Update print/printlist: display TENS values as [ e1 e2 ... ]
- [ ] Update gc: trace and collect tensors on the tensor heap
- [ ] Implement shape primitive: (shape, [t]) → vector of dimensions
- [ ] Implement rank primitive: (rank, [t]) → scalar
- [ ] Implement slice primitive: (slice, [t, i]) → element or sub-tensor at index i
- [ ] Implement head/tail sugar: head = slice 0, tail = slice 1:end
- [ ] Wire r2_maths.h for vec2/vec4/mat3/mat4 fast paths on common tensor shapes
- [ ] Implement matmul primitive using r2_maths mat_mul for general case
- [ ] Update arithmetic primitives (+, -, *, /) to operate element-wise on TENS
- [ ] Implement scalar broadcast in arithmetic (scalar op tensor)
- [ ] Add tensor? predicate primitive
- [ ] Write integration tests for tensor literals, shape, rank, slice
- [ ] Write integration tests for arithmetic and matmul on tensors
- [ ] Update README with syntax overview and examples

## 📝 Todo

## 👨‍💻 Doing (0)

## ✅ Done

**Complete**
- [x] Include r2_strings.h and add R2_STRINGS_IMPLEMENTATION to one .c file
- [x] Update scanner: use utf8_len() to detect multi-byte sequences and collect all bytes before token boundary checks
- [x] Update atom buffer: ensure buf[] is large enough for multi-byte atom names (emoji = 4 bytes each)
- [x] Update atom interning: verify strcmp/strlen still work correctly on raw UTF-8 bytes in the heap
- [x] Write tests: define and call lambdas with emoji names (e.g. 🔥, λ, π)

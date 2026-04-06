;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; gpt2_generate.lisp
;; Autoregressive text generation with GPT-2 (124M) in basis Lisp.
;;
;; Run: basis -f test_data/gpt2_generate.lisp
;;
;; Loads weights from models/gpt2.Q4_0.gguf, tokenizes the prompt,
;; then generates N_TOKENS new tokens one at a time, printing each
;; decoded token as it is produced.
;;
;; Uses 12-head attention (head-dim=64) with causal mask — matches reference GPT-2 attention.

(load "test_data/mod_transformer.lisp")

(print "loading weights...")
(load-gguf "models/gpt2.Q4_0.gguf")

(print "loading vocab...")
(load-gguf-vocab "models/gpt2.Q4_0.gguf")

(print "ready")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Hyperparameters

(define n-embd   768)
(define n-heads  12)
(define head-dim 64)
(define n-ff     3072)
(define n-vocab  50257)
(define eps      0.00001)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; GPT-2 building blocks

(define gpt2-ln (lambda (x w b)
    (+ (* (layer-norm x eps) w) b)))

(define embed (lambda (tok pos)
    (+ (slice token_embd.weight tok)
       (slice position_embd.weight pos))))

;; Extract columns [start, end) from a rank-2 matrix.
;; Works by transposing (so rows become cols), slicing rows, transposing back.
(define col-range (lambda (M start end)
    (T (slice-range (T M) start end))))

;; Scaled dot-product attention for one head.
;; Qh, Kh, Vh: (seq x head-dim); mask: (seq x seq) causal mask.
;; Returns (seq x head-dim).
(define sdpa (lambda (Qh Kh Vh mask)
    (let* (sc (+ (/ (@ Qh (T Kh)) (sqrt head-dim)) mask))
    (@ (softmax sc) Vh))))

;; Compute head h and return it transposed to (head-dim x seq).
;; Q, K, V: (seq x n-embd); h: head index 0..11.
(define head-t (lambda (Q K V mask h)
    (let* (s (* h head-dim))
    (let* (e (+ s head-dim))
    (T (sdpa (col-range Q s e)
             (col-range K s e)
             (col-range V s e)
             mask))))))

;; Stack all 12 heads into (n-embd x seq) by vstacking (head-dim x seq) slices,
;; then the caller transposes to (seq x n-embd).
(define head-stack (lambda (Q K V mask h)
    (let* (hout (head-t Q K V mask h))
    (cond
        ((< h 11) (vstack hout (head-stack Q K V mask (+ h 1))))
        (#t hout)))))

;; Multi-head self-attention: splits Q/K/V into n-heads heads of head-dim each,
;; runs scaled dot-product attention per head with causal mask, then projects.
(define gpt2-attn (lambda (x Wqkv bqkv Wo bo)
    (let* (qkv  (+ (@ x (T Wqkv)) bqkv))
    (let* (Q    (col-range qkv 0       n-embd))
    (let* (K    (col-range qkv n-embd  (* 2 n-embd)))
    (let* (V    (col-range qkv (* 2 n-embd) (* 3 n-embd)))
    (let* (seq  (slice (shape x) 0))
    (let* (mask (causal-mask seq))
    (let* (out  (T (head-stack Q K V mask 0)))
    (+ (@ out (T Wo)) bo))))))))))

(define gpt2-ff (lambda (x Wup bup Wdown bdown)
    (+ (@ (gelu (+ (@ x (T Wup)) bup)) (T Wdown)) bdown)))

(define gpt2-block (lambda (x Wln1 bln1 Wqkv bqkv Wo bo Wln2 bln2 Wup bup Wdown bdown)
    (let* (a  (gpt2-attn (gpt2-ln x Wln1 bln1) Wqkv bqkv Wo bo))
    (let* (x1 (+ x a))
    (let* (f  (gpt2-ff (gpt2-ln x1 Wln2 bln2) Wup bup Wdown bdown))
    (+ x1 f))))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Build (seq x 768) embedding matrix from a Lisp list of token IDs.
;; Uses vstack to accumulate one row per token.

(define build-input-loop (lambda (toks pos acc)
    (cond
        ((eq? toks ()) acc)
        (#t (build-input-loop (cdr toks) (+ pos 1)
                (vstack acc (reshape (embed (car toks) pos) [1 768])))))))

(define build-input (lambda (tok-list)
    (build-input-loop (cdr tok-list) 1
        (reshape (embed (car tok-list) 0) [1 768]))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Run all 12 transformer blocks on (seq x 768) hidden states.

(define run-blocks (lambda (x)
    (let* (x (gpt2-block x
        blk.0.attn_norm.weight  blk.0.attn_norm.bias
        blk.0.attn_qkv.weight   blk.0.attn_qkv.bias
        blk.0.attn_output.weight blk.0.attn_output.bias
        blk.0.ffn_norm.weight   blk.0.ffn_norm.bias
        blk.0.ffn_up.weight     blk.0.ffn_up.bias
        blk.0.ffn_down.weight   blk.0.ffn_down.bias))
    (let* (x (gpt2-block x
        blk.1.attn_norm.weight  blk.1.attn_norm.bias
        blk.1.attn_qkv.weight   blk.1.attn_qkv.bias
        blk.1.attn_output.weight blk.1.attn_output.bias
        blk.1.ffn_norm.weight   blk.1.ffn_norm.bias
        blk.1.ffn_up.weight     blk.1.ffn_up.bias
        blk.1.ffn_down.weight   blk.1.ffn_down.bias))
    (let* (x (gpt2-block x
        blk.2.attn_norm.weight  blk.2.attn_norm.bias
        blk.2.attn_qkv.weight   blk.2.attn_qkv.bias
        blk.2.attn_output.weight blk.2.attn_output.bias
        blk.2.ffn_norm.weight   blk.2.ffn_norm.bias
        blk.2.ffn_up.weight     blk.2.ffn_up.bias
        blk.2.ffn_down.weight   blk.2.ffn_down.bias))
    (let* (x (gpt2-block x
        blk.3.attn_norm.weight  blk.3.attn_norm.bias
        blk.3.attn_qkv.weight   blk.3.attn_qkv.bias
        blk.3.attn_output.weight blk.3.attn_output.bias
        blk.3.ffn_norm.weight   blk.3.ffn_norm.bias
        blk.3.ffn_up.weight     blk.3.ffn_up.bias
        blk.3.ffn_down.weight   blk.3.ffn_down.bias))
    (let* (x (gpt2-block x
        blk.4.attn_norm.weight  blk.4.attn_norm.bias
        blk.4.attn_qkv.weight   blk.4.attn_qkv.bias
        blk.4.attn_output.weight blk.4.attn_output.bias
        blk.4.ffn_norm.weight   blk.4.ffn_norm.bias
        blk.4.ffn_up.weight     blk.4.ffn_up.bias
        blk.4.ffn_down.weight   blk.4.ffn_down.bias))
    (let* (x (gpt2-block x
        blk.5.attn_norm.weight  blk.5.attn_norm.bias
        blk.5.attn_qkv.weight   blk.5.attn_qkv.bias
        blk.5.attn_output.weight blk.5.attn_output.bias
        blk.5.ffn_norm.weight   blk.5.ffn_norm.bias
        blk.5.ffn_up.weight     blk.5.ffn_up.bias
        blk.5.ffn_down.weight   blk.5.ffn_down.bias))
    (let* (x (gpt2-block x
        blk.6.attn_norm.weight  blk.6.attn_norm.bias
        blk.6.attn_qkv.weight   blk.6.attn_qkv.bias
        blk.6.attn_output.weight blk.6.attn_output.bias
        blk.6.ffn_norm.weight   blk.6.ffn_norm.bias
        blk.6.ffn_up.weight     blk.6.ffn_up.bias
        blk.6.ffn_down.weight   blk.6.ffn_down.bias))
    (let* (x (gpt2-block x
        blk.7.attn_norm.weight  blk.7.attn_norm.bias
        blk.7.attn_qkv.weight   blk.7.attn_qkv.bias
        blk.7.attn_output.weight blk.7.attn_output.bias
        blk.7.ffn_norm.weight   blk.7.ffn_norm.bias
        blk.7.ffn_up.weight     blk.7.ffn_up.bias
        blk.7.ffn_down.weight   blk.7.ffn_down.bias))
    (let* (x (gpt2-block x
        blk.8.attn_norm.weight  blk.8.attn_norm.bias
        blk.8.attn_qkv.weight   blk.8.attn_qkv.bias
        blk.8.attn_output.weight blk.8.attn_output.bias
        blk.8.ffn_norm.weight   blk.8.ffn_norm.bias
        blk.8.ffn_up.weight     blk.8.ffn_up.bias
        blk.8.ffn_down.weight   blk.8.ffn_down.bias))
    (let* (x (gpt2-block x
        blk.9.attn_norm.weight  blk.9.attn_norm.bias
        blk.9.attn_qkv.weight   blk.9.attn_qkv.bias
        blk.9.attn_output.weight blk.9.attn_output.bias
        blk.9.ffn_norm.weight   blk.9.ffn_norm.bias
        blk.9.ffn_up.weight     blk.9.ffn_up.bias
        blk.9.ffn_down.weight   blk.9.ffn_down.bias))
    (let* (x (gpt2-block x
        blk.10.attn_norm.weight  blk.10.attn_norm.bias
        blk.10.attn_qkv.weight   blk.10.attn_qkv.bias
        blk.10.attn_output.weight blk.10.attn_output.bias
        blk.10.ffn_norm.weight   blk.10.ffn_norm.bias
        blk.10.ffn_up.weight     blk.10.ffn_up.bias
        blk.10.ffn_down.weight   blk.10.ffn_down.bias))
    (let* (x (gpt2-block x
        blk.11.attn_norm.weight  blk.11.attn_norm.bias
        blk.11.attn_qkv.weight   blk.11.attn_qkv.bias
        blk.11.attn_output.weight blk.11.attn_output.bias
        blk.11.ffn_norm.weight   blk.11.ffn_norm.bias
        blk.11.ffn_up.weight     blk.11.ffn_up.bias
        blk.11.ffn_down.weight   blk.11.ffn_down.bias))
    x))))))))))))))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; One forward pass: embedding matrix -> next token ID (scalar).
;; Takes (seq x 768) directly so the caller controls context accumulation.

(define gpt2-forward (lambda (x)
    (let* (x      (run-blocks x))
    (let* (seq    (slice (shape x) 0))
    (let* (last-h (reshape (slice x (- seq 1)) [1 768]))
    (let* (normed (gpt2-ln last-h output_norm.weight output_norm.bias))
    (let* (logits (@ normed (T output.weight)))
    (argmax logits))))))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Autoregressive generation loop.
;;
;; context is a global (seq x 768) tensor updated with set! before each gc().
;; This is intentional: gc() resets the Lisp cell stack to the global env,
;; which would corrupt any cons-list passed as a local variable.  Storing
;; context as a global TENS means gc_tensors() keeps it alive while freeing
;; all the per-block intermediates from gpt2-forward.
;;
;; n and pos are plain scalars — safe to read from the (now-freed) let* cells
;; before any new allocation overwrites them, which is all we need.

(define generate (lambda (n pos)
    (cond
        ((< 0 n)
         (let* (next (gpt2-forward context))
         (let* (_    (print (token->str next)))
         (let* (_    (set! context (vstack context (reshape (embed next pos) [1 768]))))
         (let* (_    (gc))
         (generate (- n 1) (+ pos 1)))))))
        (#t ()))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Convert a rank-1 tensor to a Lisp list of scalars (needed by build-input).

(define t2l-loop (lambda (t i n)
    (cond
        ((< i n) (cons (slice t i) (t2l-loop t (+ i 1) n)))
        (#t ()))))

(define tensor->list (lambda (t)
    (t2l-loop t 0 (slice (shape t) 0))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Entry point

(define N_TOKENS 10)
(define prompt "once upon a time ")

(print "--- prompt ---")
(print prompt)

(define start-toks (tokenize prompt))
(print "token ids:")
(print start-toks)

;; Build initial (seq x 768) context from prompt tokens, store as global.
(define context (build-input (tensor->list start-toks)))

(print "--- generation ---")
(generate N_TOKENS (slice (shape context) 0))
(print "--- done ---")

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
;; Note: uses single-head attention (d_k=768) — correct pipeline,
;; outputs differ from reference GPT-2.  See cust_gpt2.lisp for details.

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
;; GPT-2 building blocks  (same as cust_gpt2.lisp)

(define gpt2-ln (lambda (x w b)
    (+ (* (layer-norm x eps) w) b)))

(define embed (lambda (tok pos)
    (+ (col-slice token_embd.weight tok)
       (col-slice position_embd.weight pos))))

(define gpt2-attn (lambda (x Wqkv bqkv Wo bo)
    (let* (qkv  (+ (@ x Wqkv) bqkv))
    (let* (qkvT (T qkv))
    (let* (Q    (T (slice-range qkvT 0    768)))
    (let* (K    (T (slice-range qkvT 768  1536)))
    (let* (V    (T (slice-range qkvT 1536 2304)))
    (let* (sc   (/ (@ Q (T K)) (sqrt n-embd)))
    (let* (attn (@ (softmax sc) V))
    (+ (@ attn Wo) bo))))))))))

(define gpt2-ff (lambda (x Wup bup Wdown bdown)
    (+ (@ (gelu (+ (@ x Wup) bup)) Wdown) bdown)))

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
;; One forward pass: token list -> next token ID (scalar).
;; All intermediate tensors are local to this function so
;; they become unreachable (and GC-collectable) on return.

(define gpt2-next (lambda (tok-list)
    (let* (x      (build-input tok-list))
    (let* (x      (run-blocks x))
    (let* (seq    (slice (shape x) 0))
    (let* (last-h (reshape (slice x (- seq 1)) [1 768]))
    (let* (normed (gpt2-ln last-h output_norm.weight output_norm.bias))
    (let* (logits (@ normed output.weight))
    (argmax logits)))))))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; List utilities

; Append a single item to the end of a list.
(define append-tok (lambda (lst tok)
    (cond
        ((eq? lst ()) (cons tok ()))
        (#t (cons (car lst) (append-tok (cdr lst) tok))))))

; Convert a rank-1 tensor to a Lisp list of scalars.
; Uses (< i n) to avoid off-by-one with float equality.
(define t2l-loop (lambda (t i n)
    (cond
        ((< i n) (cons (slice t i) (t2l-loop t (+ i 1) n)))
        (#t ()))))

(define tensor->list (lambda (t)
    (t2l-loop t 0 (slice (shape t) 0))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Autoregressive generation loop.
;; Prints each new token as it is produced.
;; Uses nested let* to sequence print → gc → recurse
;; (tinylisp let* bodies are single expressions).

(define generate (lambda (tok-list n)
    (cond
        ((eq? n 0) ())
        (#t
         (let* (next  (gpt2-next tok-list))
         (let* (s     (print (token->str next)))
         (let* (dummy (gc))
         (generate (append-tok tok-list next) (- n 1)))))))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Entry point

(define N_TOKENS 100)
(define prompt "once upon a time ")

(print "--- prompt ---")
(print prompt)

(define start-toks (tokenize prompt))
(print "token ids:")
(print start-toks)

(define start-list (tensor->list start-toks))

(print "--- generation ---")
(generate start-list N_TOKENS)
(print "--- done ---")

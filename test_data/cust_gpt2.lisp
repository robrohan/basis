;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; cust_gpt2.lisp
;; GPT-2 (124M) forward pass in basis Lisp.
;;
;; Run: basis -f test_data/cust_gpt2.lisp
;;
;; Loads weights from models/gpt2.Q4_0.gguf then runs a forward pass
;; over a short token sequence and prints the predicted next token id.
;;
;; GPT-2 architecture (from the GGUF tensors):
;;   n-embd  = 768    n-heads = 12   head-dim = 64
;;   n-ff    = 3072   n-vocab = 50257
;;   12 transformer blocks, each with:
;;     attn_qkv.weight [768 x 2304]  combined Q/K/V projection
;;     attn_qkv.bias   [2304]
;;     attn_output.weight [768 x 768]
;;     attn_output.bias   [768]
;;     attn_norm.weight / .bias      [768]  pre-attention layernorm
;;     ffn_norm.weight  / .bias      [768]  pre-ffn layernorm
;;     ffn_up.weight    [768 x 3072]
;;     ffn_up.bias      [3072]
;;     ffn_down.weight  [3072 x 768]
;;     ffn_down.bias    [768]
;;   output_norm.weight / .bias  [768]
;;   output.weight               [768 x 50257]
;;   token_embd.weight           [768 x 50257]  (col per token)
;;   position_embd.weight        [768 x 1024]   (col per position)
;;
;; GGUF weight convention: matrices are stored transposed relative to
;; PyTorch.  token_embd.weight[768 x 50257] means col j = embedding for
;; token j.  We use (col-slice M j) to extract a single column cheaply.
;; For projection matrices already shaped (in x out), plain @ works.

(load "test_data/mod_transformer.lisp")

(print "loading weights...")
(load-gguf "models/gpt2.Q4_0.gguf")
(print "weights loaded")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Hyperparameters

(define n-embd   768)
(define n-heads  12)
(define head-dim 64)     ; n-embd / n-heads
(define n-ff     3072)
(define n-vocab  50257)
(define eps      0.00001)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Building blocks

; LayerNorm with learned scale w and shift b.
; x: (seq x n-embd),  w b: (n-embd,)
; row-broadcast * and + handle (seq x n-embd) OP (n-embd,)
(define gpt2-ln (lambda (x w b)
    (+ (* (layer-norm x eps) w) b)))

; Lookup token + position embedding, return (n-embd,) vector.
; Uses col-slice to avoid transposing the full 50257-col table.
(define embed (lambda (tok pos)
    (+ (col-slice token_embd.weight tok)
       (col-slice position_embd.weight pos))))

; Single-head attention (no head splitting — uses full d=768 as one head).
; This is a simplification of GPT-2's 12-head attention; it runs correctly
; through the pipeline but produces different scores than the real model.
; Proper multi-head would split QKV into 12 x (seq x 64) heads.
;
; x:    (seq x 768)
; Wqkv: (768 x 2304),  bqkv: (2304,)
; Wo:   (768 x 768),   bo:   (768,)
(define gpt2-attn (lambda (x Wqkv bqkv Wo bo)
    (let* (qkv  (+ (@ x Wqkv) bqkv))          ; (seq x 2304)
    (let* (qkvT (T qkv))                       ; (2304 x seq) for row-range split
    (let* (Q    (T (slice-range qkvT 0    768))); (seq x 768)
    (let* (K    (T (slice-range qkvT 768  1536))); (seq x 768)
    (let* (V    (T (slice-range qkvT 1536 2304))); (seq x 768)
    (let* (sc   (/ (@ Q (T K)) (sqrt n-embd))) ; scale by sqrt(d_k)
    (let* (attn (@ (softmax sc) V))            ; (seq x 768)
    (+ (@ attn Wo) bo))))))))))               ; (seq x 768)

; Feed-forward block: up-project with gelu, down-project back.
; x: (seq x 768)
(define gpt2-ff (lambda (x Wup bup Wdown bdown)
    (+ (@ (gelu (+ (@ x Wup) bup)) Wdown) bdown)))

; One complete GPT-2 transformer block.
; All weights passed explicitly so the same function works for every layer.
(define gpt2-block (lambda (x Wln1 bln1 Wqkv bqkv Wo bo Wln2 bln2 Wup bup Wdown bdown)
    (let* (a (gpt2-attn (gpt2-ln x Wln1 bln1) Wqkv bqkv Wo bo))
    (let* (x1 (+ x a))
    (let* (f (gpt2-ff (gpt2-ln x1 Wln2 bln2) Wup bup Wdown bdown))
    (+ x1 f))))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Forward pass

; Build (seq x 768) input from a list of token ids.
; Tokens: [15496 11 314 716] = "Hello, I am" in GPT-2 BPE
(define tok0 15496)
(define tok1 11)
(define tok2 314)
(define tok3 716)

(print "building input embeddings...")
(define x (make-tensor (embed tok0 0)
                       (embed tok1 1)
                       (embed tok2 2)
                       (embed tok3 3)))
(print "input shape:")
(print (shape x))

; Run all 12 transformer blocks
(print "running block 0...")
(define x (gpt2-block x
    blk.0.attn_norm.weight  blk.0.attn_norm.bias
    blk.0.attn_qkv.weight   blk.0.attn_qkv.bias
    blk.0.attn_output.weight blk.0.attn_output.bias
    blk.0.ffn_norm.weight   blk.0.ffn_norm.bias
    blk.0.ffn_up.weight     blk.0.ffn_up.bias
    blk.0.ffn_down.weight   blk.0.ffn_down.bias))

(print "running block 1...")
(define x (gpt2-block x
    blk.1.attn_norm.weight  blk.1.attn_norm.bias
    blk.1.attn_qkv.weight   blk.1.attn_qkv.bias
    blk.1.attn_output.weight blk.1.attn_output.bias
    blk.1.ffn_norm.weight   blk.1.ffn_norm.bias
    blk.1.ffn_up.weight     blk.1.ffn_up.bias
    blk.1.ffn_down.weight   blk.1.ffn_down.bias))

(print "running block 2...")
(define x (gpt2-block x
    blk.2.attn_norm.weight  blk.2.attn_norm.bias
    blk.2.attn_qkv.weight   blk.2.attn_qkv.bias
    blk.2.attn_output.weight blk.2.attn_output.bias
    blk.2.ffn_norm.weight   blk.2.ffn_norm.bias
    blk.2.ffn_up.weight     blk.2.ffn_up.bias
    blk.2.ffn_down.weight   blk.2.ffn_down.bias))

(print "running block 3...")
(define x (gpt2-block x
    blk.3.attn_norm.weight  blk.3.attn_norm.bias
    blk.3.attn_qkv.weight   blk.3.attn_qkv.bias
    blk.3.attn_output.weight blk.3.attn_output.bias
    blk.3.ffn_norm.weight   blk.3.ffn_norm.bias
    blk.3.ffn_up.weight     blk.3.ffn_up.bias
    blk.3.ffn_down.weight   blk.3.ffn_down.bias))

(print "running block 4...")
(define x (gpt2-block x
    blk.4.attn_norm.weight  blk.4.attn_norm.bias
    blk.4.attn_qkv.weight   blk.4.attn_qkv.bias
    blk.4.attn_output.weight blk.4.attn_output.bias
    blk.4.ffn_norm.weight   blk.4.ffn_norm.bias
    blk.4.ffn_up.weight     blk.4.ffn_up.bias
    blk.4.ffn_down.weight   blk.4.ffn_down.bias))

(print "running block 5...")
(define x (gpt2-block x
    blk.5.attn_norm.weight  blk.5.attn_norm.bias
    blk.5.attn_qkv.weight   blk.5.attn_qkv.bias
    blk.5.attn_output.weight blk.5.attn_output.bias
    blk.5.ffn_norm.weight   blk.5.ffn_norm.bias
    blk.5.ffn_up.weight     blk.5.ffn_up.bias
    blk.5.ffn_down.weight   blk.5.ffn_down.bias))

(print "running block 6...")
(define x (gpt2-block x
    blk.6.attn_norm.weight  blk.6.attn_norm.bias
    blk.6.attn_qkv.weight   blk.6.attn_qkv.bias
    blk.6.attn_output.weight blk.6.attn_output.bias
    blk.6.ffn_norm.weight   blk.6.ffn_norm.bias
    blk.6.ffn_up.weight     blk.6.ffn_up.bias
    blk.6.ffn_down.weight   blk.6.ffn_down.bias))

(print "running block 7...")
(define x (gpt2-block x
    blk.7.attn_norm.weight  blk.7.attn_norm.bias
    blk.7.attn_qkv.weight   blk.7.attn_qkv.bias
    blk.7.attn_output.weight blk.7.attn_output.bias
    blk.7.ffn_norm.weight   blk.7.ffn_norm.bias
    blk.7.ffn_up.weight     blk.7.ffn_up.bias
    blk.7.ffn_down.weight   blk.7.ffn_down.bias))

(print "running block 8...")
(define x (gpt2-block x
    blk.8.attn_norm.weight  blk.8.attn_norm.bias
    blk.8.attn_qkv.weight   blk.8.attn_qkv.bias
    blk.8.attn_output.weight blk.8.attn_output.bias
    blk.8.ffn_norm.weight   blk.8.ffn_norm.bias
    blk.8.ffn_up.weight     blk.8.ffn_up.bias
    blk.8.ffn_down.weight   blk.8.ffn_down.bias))

(print "running block 9...")
(define x (gpt2-block x
    blk.9.attn_norm.weight  blk.9.attn_norm.bias
    blk.9.attn_qkv.weight   blk.9.attn_qkv.bias
    blk.9.attn_output.weight blk.9.attn_output.bias
    blk.9.ffn_norm.weight   blk.9.ffn_norm.bias
    blk.9.ffn_up.weight     blk.9.ffn_up.bias
    blk.9.ffn_down.weight   blk.9.ffn_down.bias))

(print "running block 10...")
(define x (gpt2-block x
    blk.10.attn_norm.weight  blk.10.attn_norm.bias
    blk.10.attn_qkv.weight   blk.10.attn_qkv.bias
    blk.10.attn_output.weight blk.10.attn_output.bias
    blk.10.ffn_norm.weight   blk.10.ffn_norm.bias
    blk.10.ffn_up.weight     blk.10.ffn_up.bias
    blk.10.ffn_down.weight   blk.10.ffn_down.bias))

(print "running block 11...")
(define x (gpt2-block x
    blk.11.attn_norm.weight  blk.11.attn_norm.bias
    blk.11.attn_qkv.weight   blk.11.attn_qkv.bias
    blk.11.attn_output.weight blk.11.attn_output.bias
    blk.11.ffn_norm.weight   blk.11.ffn_norm.bias
    blk.11.ffn_up.weight     blk.11.ffn_up.bias
    blk.11.ffn_down.weight   blk.11.ffn_down.bias))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Output: logits for last token

(print "computing logits...")

; Apply final layer norm to last token's hidden state
(define last-hidden (reshape (slice x 3) [1 768]))  ; (1 x 768)
(define normed (gpt2-ln last-hidden output_norm.weight output_norm.bias))

; Project to vocabulary: (1 x 768) @ (768 x 50257) = (1 x 50257)
(define logits (@ normed output.weight))

; Predicted next token = argmax over vocab
(define next-tok (argmax logits))
(print "predicted next token id:")
(print next-tok)

; Show the top logit value as a sanity check
(print "max logit value:")
(print (amax logits))

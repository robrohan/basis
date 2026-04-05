;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; mod_transformer.lisp
;; Transformer building blocks in basis Lisp.
;;
;; C primitives used: @ T softmax layer-norm sum amax log sqrt abs
;;
;; Convention: sequences are (seq_len x d_model) matrices — one token per row.
;; Bias terms are omitted (weights assumed pre-biased or bias-free) to avoid
;; needing row-vector broadcasting.  Add bias with (+ (@ x W) b) once
;; broadcasting is supported.

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Activations (in Lisp on top of existing primitives)

; relu(x) = 0.5 * (x + |x|)  — works element-wise on scalars and tensors
(define relu (lambda (x) (* 0.5 (+ x (abs x)))))

; gelu approx: 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
; tanh(y) = (exp(2y) - 1) / (exp(2y) + 1)
(define tanh- (lambda (y)
    (let* (e2y (exp (* 2.0 y)))
    (/ (- e2y 1.0) (+ e2y 1.0)))))

(define gelu (lambda (x)
    (* (* 0.5 x)
       (+ 1.0 (tanh- (* 0.7978845608 (+ x (* 0.044715 (* x (* x x))))))))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Scaled dot-product attention
;;
;; Q, K, V: (seq_len x d_k) matrices
;; mask (optional causal mask): not yet implemented — pass nil
;; Returns (seq_len x d_k) context vectors
;;
;;   scores = Q @ K^T / sqrt(d_k)
;;   weights = softmax(scores)       ; row-wise: each query attends to all keys
;;   output  = weights @ V
(define sdp-attention (lambda (Q K V)
    (let* (d_k (slice (shape Q) 1))
    (let* (scores (/ (@ Q (T K)) (sqrt d_k)))
    (@ (softmax scores) V)))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Single attention head
;;
;; x:       (seq_len x d_model) input
;; Wq Wk Wv: (d_model x d_k)   projection matrices
;; Returns (seq_len x d_k)
(define attention-head (lambda (x Wq Wk Wv)
    (sdp-attention (@ x Wq) (@ x Wk) (@ x Wv))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Feed-forward block (two linear layers with relu)
;;
;; x:  (seq_len x d_model)
;; W1: (d_model x d_ff)
;; W2: (d_ff x d_model)
;; Returns (seq_len x d_model)
(define feed-forward (lambda (x W1 W2)
    (@ (relu (@ x W1)) W2)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Single transformer block (post-norm, GPT-style)
;;
;; x:       (seq_len x d_model) input
;; Wq Wk Wv: (d_model x d_k)   attention projections
;; Wo:      (d_k x d_model)     attention output projection
;; W1:      (d_model x d_ff)    ff expand
;; W2:      (d_ff x d_model)    ff contract
;; eps:     small float for layer-norm stability (e.g. 1e-5)
;;
;; Returns (seq_len x d_model)
(define transformer-block (lambda (x Wq Wk Wv Wo W1 W2 eps)
    (let* (attn (@ (attention-head x Wq Wk Wv) Wo))
    (let* (x1   (layer-norm (+ x attn) eps))
    (let* (ff   (feed-forward x1 W1 W2))
    (layer-norm (+ x1 ff) eps))))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; test_transformer.lisp
;; Smoke tests for transformer primitives and mod_transformer.lisp

(load "test_data/mod_testing.lisp")
(load "test_data/mod_transformer.lisp")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; sum

(assert (sum [1.0 2.0 3.0]) 6.0)
(assert (sum [[1.0 2.0][3.0 4.0]]) 10.0)
(assert (sum 5.0) 5.0)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; amax

(assert (amax [1.0 5.0 3.0]) 5.0)
(assert (amax [[1.0 9.0][3.0 2.0]]) 9.0)
(assert (amax -3.0) -3.0)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; log

(assert (log 1.0) 0.0)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; softmax: values in (0,1) and sum to 1

(define sm1 (softmax [1.0 2.0 3.0]))

; each weight in (0,1)
(assert (< (slice sm1 0) 1.0) #t)
(assert (> (slice sm1 0) 0.0) #t)

; largest input gets largest weight
(assert (> (slice sm1 2) (slice sm1 1)) #t)
(assert (> (slice sm1 1) (slice sm1 0)) #t)

; row-wise softmax on 2x3: each row sums close to 1.0
(define sm2 (softmax [[1.0 2.0 3.0][4.0 5.0 6.0]]))
(assert (< (abs (- (sum (slice sm2 0)) 1.0)) 0.0001) #t)
(assert (< (abs (- (sum (slice sm2 1)) 1.0)) 0.0001) #t)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; layer-norm: output mean ~ 0

(define ln1 (layer-norm [1.0 2.0 3.0 4.0] 0.00001))
(assert (< (abs (/ (sum ln1) 4.0)) 0.0001) #t)

; row-wise layer-norm
(define ln2 (layer-norm [[1.0 2.0 3.0 4.0][10.0 20.0 30.0 40.0]] 0.00001))
(assert (< (abs (/ (sum (slice ln2 0)) 4.0)) 0.0001) #t)
(assert (< (abs (/ (sum (slice ln2 1)) 4.0)) 0.0001) #t)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; relu (Lisp implementation)

(assert (relu 3.0) 3.0)
(assert (relu -2.0) 0.0)
(assert (relu 0.0) 0.0)
(assert (slice (relu [-1.0 0.0 2.0]) 0) 0.0)
(assert (slice (relu [-1.0 0.0 2.0]) 2) 2.0)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; sdp-attention: output shape (2 x 4)

(define Q [[1.0 0.0 0.0 0.0][0.0 1.0 0.0 0.0]])
(define K [[1.0 0.0 0.0 0.0][0.0 1.0 0.0 0.0]])
(define V [[1.0 2.0 3.0 4.0][5.0 6.0 7.0 8.0]])

(define attn-out (sdp-attention Q K V))
(assert (rank attn-out) 2)
(assert (slice (shape attn-out) 0) 2.0)
(assert (slice (shape attn-out) 1) 4.0)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; transformer-block: output shape preserved (seq_len=2, d_model=4)

; identity weights: d_model=4, d_k=4, d_ff=8
(define Wq [[1.0 0.0 0.0 0.0][0.0 1.0 0.0 0.0][0.0 0.0 1.0 0.0][0.0 0.0 0.0 1.0]])
(define Wk [[1.0 0.0 0.0 0.0][0.0 1.0 0.0 0.0][0.0 0.0 1.0 0.0][0.0 0.0 0.0 1.0]])
(define Wv [[1.0 0.0 0.0 0.0][0.0 1.0 0.0 0.0][0.0 0.0 1.0 0.0][0.0 0.0 0.0 1.0]])
(define Wo [[1.0 0.0 0.0 0.0][0.0 1.0 0.0 0.0][0.0 0.0 1.0 0.0][0.0 0.0 0.0 1.0]])
(define W1 [[1.0 0.0 0.0 0.0 0.0 0.0 0.0 0.0]
            [0.0 1.0 0.0 0.0 0.0 0.0 0.0 0.0]
            [0.0 0.0 1.0 0.0 0.0 0.0 0.0 0.0]
            [0.0 0.0 0.0 1.0 0.0 0.0 0.0 0.0]])
(define W2 [[1.0 0.0 0.0 0.0]
            [0.0 1.0 0.0 0.0]
            [0.0 0.0 1.0 0.0]
            [0.0 0.0 0.0 1.0]
            [0.0 0.0 0.0 0.0]
            [0.0 0.0 0.0 0.0]
            [0.0 0.0 0.0 0.0]
            [0.0 0.0 0.0 0.0]])

(define x-in [[1.0 2.0 3.0 4.0][5.0 6.0 7.0 8.0]])
(define blk-out (transformer-block x-in Wq Wk Wv Wo W1 W2 0.00001))

(assert (rank blk-out) 2)
(assert (slice (shape blk-out) 0) 2.0)
(assert (slice (shape blk-out) 1) 4.0)

(print "transformer tests passed")

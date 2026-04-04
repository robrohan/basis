;;; homoiconicity_test.lisp -- tensors as code-as-data
;;; Run with:
;;;    ./build/Darwin/arm64/basis.debug -f test_data/homoiconicity_test.lisp

(load "./test_data/mod_testing.lisp")

(print "-- s-expressions inside tensor literals --")

					; ---- s-expressions inside tensor literals ----
					; expressions are evaluated when the tensor is built, not
					; at parse time
(define x 3)

(assert [(+ 3 x) x] [6 3])
					; => [6 3]

(assert [[1 x] [x (+ x 1)]] [[1 3] [3 4]])
					; => [[1 3] [3 4]]


					; ---- quote stores the tensor form as unevaluated code ----
					; a quoted tensor literal is just a (make-tensor ...) list,
					; not a tensor yet
(define code '[1 2 3])
(print code)
					; => (make-tensor 1 2 3)    <-- it's code, not data yet

(assert (eval code) [1 2 3])
					; => [1 2 3]

					; ---- deferred evaluation uses the environment at eval time ----
					; the expression captures variable names, not their current values
(define template '[(+ 10 x) x])
(define x 4)
(assert (eval template) [14 4])
					; => [14 4]

(define x 7)
(assert (eval template) [17 7])
					; => [17 7]

					; ---- lambda with a tensor body ----
					; the [a b] literal becomes (make-tensor a b) which is
					; evaluated each call
(define make-row (lambda (a b) [a b]))
(assert (make-row 5 6) [5 6])
					; => [5 6]

(assert (make-row 100 200) [100 200])
					; => [100 200]

					; ---- building a matrix from rows via lambda ----
(define make-identity-2 (lambda ()
  [[(+ 1 0) 0]
   [0       (+ 1 0)]]))
(assert (make-identity-2) [[1 0] [0 1]])
					; => [[1 0] [0 1]]

					; ---- building code programmatically and evaluating it ----
					; cons together a make-tensor call and eval it

(define row (cons 'make-tensor (cons 9 (cons 8 (cons 7 '())))))
(print row)
					; => (make-tensor 9 8 7)
(assert (eval row) [9 8 7])
					; => [9 8 7]

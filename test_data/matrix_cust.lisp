
(define M [
  [ 1 2 -3 4]
  [ 5 6 7 8]
  [ 9 0 1 2]
  ])

(define v [ 1 2 3 ])
(define q (@ v M))

					; this should work but does not
;(define b (quote [1 (+ 1 2) 1]))
(define b (quote [1 1 1]))

					; lambda seems to be good
(define 🐑 (lambda (x) (* x x)))
(🐑 3)

					; You have to set it as a variable or it
					; wont display the variables output
					; I think this is due to needing to move to
					; the "lisp" stack
(define b (slice M 0))
b
					; distance between two vectors
(dist (slice M 0) (slice M 1))



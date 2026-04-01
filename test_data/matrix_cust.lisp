(define M [
  [ 1 2 -3 4]
  [ 5 6 7 8]
  [ 9 0 1 2]
  ])

(define v [ 1 2 3 ])
(define q (@ v M))


(define b (quote [1 (+ 1 2) 1]))
(print b)
(print (eval b))

					; lambda seems to be good
(define 🐑 (lambda (x) (* x x)))
(print (🐑 3))

(define b (slice M 0))
(print (slice M 0))
(print b)
					; distance between two vectors
(print (dist (slice M 0) (slice M 1)))



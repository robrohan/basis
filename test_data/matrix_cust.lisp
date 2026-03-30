
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

(define o (dist [0 0 0] [1 3 4])
  )
o




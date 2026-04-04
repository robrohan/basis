; tensor_test.lisp — smoke tests for tensor literals and primitives
; Run with: echo '...' | ./build/Darwin/arm64/basis.debug
; or load interactively at the REPL

(load "./test_data/mod_testing.lisp")


(print "-- literals --")
					; scalar (rank 0)
(assert (rank 42) 0)
					; => 0

					; vector literal
(assert [1 2 3] [1 2 3])
					; => [1 2 3]

(print "-- rank --")
(assert (rank [42]) 1)
					; => 1   (a 1-element vector, rank-1)

(assert (rank [1 2 3]) 1)
					; => 1

(assert (rank [10 20 30 40]) 1)
					; => 1

(print "-- shape --")
(assert (shape [1 2 3]) [3])
					; => [3]

(assert (shape [10 20 30 40]) [4])
					; => [4]
(assert (shape 0) ERR)
					; => ERR

(print "-- tensor? predicate --")
(assert (tensor? [1 2 3]) #t)
					; => #t

(assert (tensor? 42) ())
					; => ()

(assert (tensor? (quote hello)) ())
					; => ()

(print "-- slice --")
(assert (slice [10 20 30] 0) 10)
					; => 10

(assert (slice [10 20 30] 1) 20)
					; => 20

(assert (slice [10 20 30] 2) 30)
					; => 30

(print "-- element-wise arithmetic --")
(assert (+ [1 2 3] [4 5 6]) [5 7 9])
					; => [5 7 9]

(assert (- [10 20 30] [1 2 3]) [9 18 27])
					; => [9 18 27]

(assert (* [2 3 4] [1 2 3]) [2 6 12])
					; => [2 6 12]

(assert (/ [10 20 30] [2 4 5]) [5 5 6])
					; => [5 5 6]

(print "-- scalar broadcast --")
(assert (+ [1 2 3] 10) [11 12 13])
					; => [11 12 13]

(assert (* [1 2 3] 2) [2 4 6])
					; => [2 4 6]

(assert (- [10 20 30] 5) [5 15 25])
					; => [5 15 25]

(assert (/ [10 20 30] 10) [1 2 3])
					; => [1 2 3]

(print "-- tensor in define / lambda --")

(define v [3 1 4 1 5])

(assert (shape v) [5])
					; => [5]

(assert (slice v 2) 4)
					; => 4

(assert (+ v [1 1 1 1 1]) [4 2 5 2 6])
					; => [4 2 5 2 6]

(print "-- nested expressions --")
(assert (+ [1 2 3]
	   (* [2 2 2] [3 4 5]))
	[7 10 13])
					; => [7 10 13]

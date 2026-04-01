; tensor_test.lisp — smoke tests for tensor literals and primitives
; Run with: echo '...' | ./build/Darwin/arm64/basis.debug
; or load interactively at the REPL

					; ---- tensor literals ----
					; scalar (rank 0)
(print (rank [42]))
					; => 1   (a 1-element vector, rank-1)

(print (rank 42))
					; => ERR (should be rank 0)

					; vector literal
(print [1 2 3])
					; => [1 2 3]

					; ---- rank ----
(print (rank [1 2 3]))
					; => 1

(print (rank [10 20 30 40]))
					; => 1

					; ---- shape ----
(print (shape [1 2 3]))
					; => [3]

(print (shape [10 20 30 40]))
					; => [4]
(print (shape 0))
					; => ERR

					; ---- tensor? predicate ----
(print (tensor? [1 2 3]))
					; => #t

(print (tensor? 42))
					; => ()

(print (tensor? (quote hello)))
					; => ()

					; ---- slice ----
(print (slice [10 20 30] 0))
					; => 10

(print (slice [10 20 30] 1))
					; => 20

(print (slice [10 20 30] 2))
					; => 30

					; ---- element-wise arithmetic ----
(print (+ [1 2 3] [4 5 6]))
					; => [5 7 9]

(print (- [10 20 30] [1 2 3]))
					; => [9 18 27]

(print (* [2 3 4] [1 2 3]))
					; => [2 6 12]

(print (/ [10 20 30] [2 4 5]))
					; => [5 5 6]

					; ---- scalar broadcast ----
(print(+ [1 2 3] 10))
					; => [11 12 13]

(print (* [1 2 3] 2))
					; => [2 4 6]

(print (- [10 20 30] 5))
					; => [5 15 25]

(print (/ [10 20 30] 10))
					; => [1 2 3]

					; ---- tensor in define / lambda ----
(define v [3 1 4 1 5])
(print (shape v))
					; => [5]

(print (slice v 2))
					; => 4

(print (+ v [1 1 1 1 1]))
					; => [4 2 5 2 6]

					; ---- nested expressions ----
(print (+ [1 2 3] (* [2 2 2] [3 4 5])))
					; => [7 10 13]

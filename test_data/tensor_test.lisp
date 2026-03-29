; tensor_test.lisp — smoke tests for tensor literals and primitives
; Run with: echo '...' | ./build/Darwin/arm64/basis.debug
; or load interactively at the REPL

; ---- tensor literals ----
; scalar (rank 0)
; (rank [42])        => 1   (a 1-element vector, not truly rank-0 yet)

; vector literal
[1 2 3]
; => [1 2 3]

; ---- rank ----
(rank [1 2 3])
; => 1

(rank [10 20 30 40])
; => 1

; ---- shape ----
(shape [1 2 3])
; => [3]

(shape [10 20 30 40])
; => [4]

; ---- tensor? predicate ----
(tensor? [1 2 3])
; => #t

(tensor? 42)
; => ()

(tensor? (quote hello))
; => ()

; ---- slice ----
(slice [10 20 30] 0)
; => 10

(slice [10 20 30] 1)
; => 20

(slice [10 20 30] 2)
; => 30

; ---- element-wise arithmetic ----
(+ [1 2 3] [4 5 6])
; => [5 7 9]

(- [10 20 30] [1 2 3])
; => [9 18 27]

(* [2 3 4] [1 2 3])
; => [2 6 12]

(/ [10 20 30] [2 4 5])
; => [5 5 6]

; ---- scalar broadcast ----
(+ [1 2 3] 10)
; => [11 12 13]

(* [1 2 3] 2)
; => [2 4 6]

(- [10 20 30] 5)
; => [5 15 25]

(/ [10 20 30] 10)
; => [1 2 3]

; ---- tensor in define / lambda ----
(define v [3 1 4 1 5])
(shape v)
; => [5]

(slice v 2)
; => 4

(+ v [1 1 1 1 1])
; => [4 2 5 2 6]

; ---- nested expressions ----
; => [7 10 13]
(+ [1 2 3] (* [2 2 2] [3 4 5]))

(quote Done)

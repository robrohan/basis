; UTF-8 / emoji atom name tests

; Greek letters as identifiers
(define π 3.14159265358979)
(define λ (lambda (x) (* x 2)))

; Emoji as identifiers
(define 🔥 (lambda (x) (* x x)))
(define 💧 (lambda (x) (/ x 2)))

; Mixed emoji and ASCII
(define fire🔥 (lambda (x) (+ x 1)))

; Emoji as an alias for another value
(define 📐 π)

; Using them
(λ 21)        ; expected: 42
(🔥 6)        ; expected: 36
(💧 100)      ; expected: 50
(fire🔥 41)   ; expected: 42
📐             ; expected: 3.14159265358979

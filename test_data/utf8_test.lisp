;; UTF-8 / emoji atom name tests

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

					; ---- Using them ----
(print (λ 21))
					; => 42
(print (🔥 6))
					; => 36

(print (💧 100))
					; => 50
(print (fire🔥 41))
					; => 42
(print 📐)
					; => 3.14159265358979

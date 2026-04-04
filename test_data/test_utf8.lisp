;; UTF-8 / emoji atom name tests

(load "./test_data/mod_testing.lisp")

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

(print "-- Using UTF8 Defined bits  --")

(assert (λ 21) 42)
					; => 42
(assert (🔥 6) 36)
					; => 36

(assert (💧 100) 50)
					; => 50
(assert (fire🔥 41) 42)
					; => 42
(assert 📐 3.14159265358979)
					; => 3.14159265358979

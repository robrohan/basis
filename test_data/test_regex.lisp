; test_regex.lisp — tests for (re-match pattern text) and (substring text start len)
; ./build/Darwin/arm64/basis.debug -f ./test_data/test_regex.lisp
;
; Lisp strings are raw bytes: one backslash in source = one backslash in pattern.
; re-match returns (start len) on match, () on no match (byte offsets).
; substring uses the same byte offsets so the two compose directly.

(load "./test_data/mod_testing.lisp")

; helper: extract the matched string from a re-match result
; re-find: extract matched string, or () on no match
; uses a helper lambda to avoid re-calling re-match
(define re-find-h (lambda (m text)
  (if m (substring text (car m) (car (cdr m))) ())))

(define re-find (lambda (pattern text)
  (re-find-h (re-match pattern text) text)))

; helper: check start and length of a match result
(define assert-match (lambda (result start len)
  (assert (car result) start)
  (assert (car (cdr result)) len)))

;;; ---- substring ----

(print "-- substring basic --")
(assert (substring "hello world" 0 5) "hello")
(assert (substring "hello world" 6 5) "world")
(assert (substring "abc123" 3 3)      "123")

(print "-- substring bounds errors --")
(assert (substring "hi" -1 1) ERR)
(assert (substring "hi"  0 5) ERR)
(assert (substring "hi"  3 1) ERR)

(print "-- substring zero length --")
(assert (substring "hello" 2 0) "")

;;; ---- re-match position ----

(print "-- no match returns () --")
(assert (re-match "\d+" "no digits here") ())
(assert (re-match "^world" "hello world")  ())
(assert (re-match "hello$" "hello world")  ())
(assert (re-match "ab+c"   "ac")           ())
(assert (re-match "h.llo"  "hllo")         ())

(print "-- digit match position --")
(assert-match (re-match "\d+" "abc123def") 3 3)

(print "-- anchors --")
(assert-match (re-match "^hello"  "hello world") 0 5)
(assert-match (re-match "world$"  "hello world") 6 5)

(print "-- character class --")
(assert-match (re-match "[a-z]+" "123abc456") 3 3)

(print "-- quantifiers --")
(assert-match (re-match "ab*c"  "ac")    0 2)
(assert-match (re-match "ab*c"  "abbbc") 0 5)
(assert-match (re-match "ab+c"  "abc")   0 3)
(assert-match (re-match "ab?c"  "ac")    0 2)
(assert-match (re-match "ab?c"  "abc")   0 3)

;;; ---- re-find: extract matched string via substring ----

(print "-- re-find extracts digits --")
(assert (re-find "\d+" "abc123def")    "123")
(assert (re-find "\d+" "price: 42 usd") "42")
(assert (re-find "\d+" "no digits")     ())

(print "-- re-find word chars --")
(assert (re-find "\w+" "  hello123  ") "hello123")

(print "-- re-find character class --")
(assert (re-find "[A-Z][a-z]+" "foo Bar baz") "Bar")

(print "-- re-find in log line --")
(define log1 "2024-04-09 10:05:22 WARN call (555) 123-4567 for help")
(assert (re-find "\d\d\d\d-\d\d-\d\d" log1) "2024-04-09")

;;; ---- UTF-8 ----

(print "-- UTF-8: ASCII pattern through multibyte chars --")
; "café" = c(1) a(1) f(1) é(2 bytes) = 5 bytes total
(assert-match (re-match "[a-z]+" "café") 0 3)
(assert       (re-find  "[a-z]+" "café") "caf")

; digit after é in "café2" — '2' is at byte offset 5
(assert-match (re-match "\d" "café2") 5 1)
(assert       (re-find  "\d" "café2") "2")

(print "done")

#!/usr/bin/env basis

(define x "Doing a loop")
(print x)

(defun inner-loop (itr) (print itr))

(defun main-loop (n)
  (if (< 0 n)
      (let*
  	(_ (inner-loop n))       ; let* always has to set to something
	(main-loop (- n 1))      ; unless tail?
	)
      (print "Done")
      ))

(main-loop 10)


#!/usr/bin/env basis

(define x "Hello World")
(print x)

(define inner-loop (lambda (itr) (print itr)))

(define main-loop (lambda (n)
		    (if (< 0 n)
			(let*
  			  (print (inner-loop n))
			  (main-loop (- n 1))
			  )
			()
		    )))

(main-loop 10)



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;

(define assert (lambda (a b)
		 (if (equal a b)
		     (print #t)
		     (print ERR))))


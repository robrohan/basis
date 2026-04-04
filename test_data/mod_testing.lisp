;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;

(define assert (lambda (a b)
		 (if (eq? a b)
		     (print #t)
		     (print ERR))))


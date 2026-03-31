;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define W1 [
  [0.5 0.5]
  [0.5 0.5]
  ])
(define b1 [0.1 0.1])
(define W2 [0.5 0.5])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define relu (lambda (x) (* 0.5 (+ x (abs x)))))
(define sigmoid (lambda (x) (/ 1 (+ 1 (exp (* -1 x))))))
(define sigmoid-d (lambda (s) (* s (- 1 s))))

(define step (lambda (x)
	       (* 0.5 (+ 1 (/ x (+ (abs x) 0.0001))))
	       ))

(define outer2 (lambda (u v)
		 (make-tensor (* (slice u 0) v)
			      (* (slice u 1) v))
		 ))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define training_input [[0 0] [0 1] [1 0] [1 1]])
(define expected_output [0 1 1 0])

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define lr 0.3)

(define train-one (lambda (x y)
		    (let*
		      (z1  (+ (@ W1 x) b1))
		      (h1  (relu z1))
		      (s   (dot W2 h1))
		      (yh  (sigmoid s))
		      (er  (* 2 (- yh y)))
		      (dout (* er (sigmoid-d yh)))
		      (dW2 (* dout h1))
		      (dh1 (* dout W2))
		      (dz1 (* dh1 (step z1)))
		      (_ (define W2 (- W2 (* lr dW2))))
		      (_ (define b1 (- b1 (* lr dz1))))
		      (define W1 (- W1 (* lr (outer2 dz1 x))))
		      (gc)
		      )
		    ))

(define train-epoch (lambda (i)
		      (if (< i 4)
			  (let* (_ (train-one (slice training_input i)
				     (slice expected_output i)))
			    (train-epoch (+ i 1))
			    (gc)
			    )
			  ()
			  )
		      ))

(define forward (lambda (input)
		  (let* (h1 (relu (+ (@ W1 input) b1)))
		    (sigmoid (dot W2 h1)))
		  ))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define predict (lambda (input)
		  (if (< 0.5 (forward input)) 1 0)))

(define train (lambda (n)
		(if (< 0 n)
		    (let* (_ (train-epoch 0))
		      (train (- n 1)))
		    ())
		))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(train 380)

(make-tensor (predict [0 0])
             (predict [0 1])
             (predict [1 0])
             (predict [1 1]))

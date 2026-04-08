;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Simple NN to solve the XOR problem
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define W1 [
  [1 1]
  [1 1]
  ])
(define b1 [-0.5 -1.5])
(define W2 [1 -2])

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
					; z1 = W1 @ x + b1
					;   matrix-multiply weights by input, add bias — pre-activation
					;   for hidden layer
		      (z1  (+ (@ W1 x) b1))
					; h1 = relu(z1)
					;   apply ReLU: clamp negatives to zero — hidden layer activations
		      (h1  (relu z1))
					; s = dot(W2, h1)
					;   weighted sum of hidden activations → single scalar
					;   (pre-activation for output)
		      (s   (dot W2 h1))
					; yh = sigmoid(s)
					;   squash scalar to (0,1) range — the network's predicted probability
		      (yh  (sigmoid s))
					; er = 2 * (yh - y)
					;   derivative of MSE loss (d/dyh of (yh-y)^2) — signed error at output
		      (er  (* 2 (- yh y)))
					; dout = er * sigmoid'(yh)
					;   chain rule: scale error by sigmoid's local slope at yh
		      (dout (* er (sigmoid-d yh)))
					; dW2 = dout * h1
					;   gradient for output weights: outer product (scalar * vector)
					;   = weight update direction
		      (dW2 (* dout h1))
					; dh1 = dout * W2
					;   backprop through output layer: distribute gradient to hidden
					;   activations
		      (dh1 (* dout W2))
					; dz1 = dh1 * step(z1)
					;   backprop through ReLU: step(z1) is 1 where neuron fired, 0 where it
					;   was clamped
		      (dz1 (* dh1 (step z1)))
					; W2 -= lr * dW2
					;   gradient descent step for output weights
		      (_ (setq W2 (- W2 (* lr dW2))))
					; b1 -= lr * dz1
					;   gradient descent step for hidden biases
		      (_ (setq b1 (- b1 (* lr dz1))))
					; W1 -= lr * outer(dz1, x)
					;   gradient descent step for input weights: outer2 builds the 2x2
					;   gradient matrix
		      (setq W1 (- W1 (* lr (outer2 dz1 x))))
		      )
		    ))

(define train-epoch (lambda (i)
		      (if (< i 4)
			  (let*
			    (_ (train-one (slice training_input i) (slice expected_output i)))
			    (train-epoch (+ i 1))
			    )
			  ()
			  )
		      ))

(define forward (lambda (input)
		  (let*
					; z1 = W1 @ input + b1
					;   matrix-multiply weights W1 by the input vector, then add bias b1
					;   result is a 2-element vector: one pre-activation value per
					;   hidden neuron
		      (h1 (relu (+ (@ W1 input) b1)))
					; h1 = relu(z1)
					;   apply ReLU activation: clamps negative values to zero
					;   each hidden neuron either "fires" (positive) or stays silent (zero)
					; return sigmoid(dot(W2, h1))
					;   dot(W2, h1): weighted sum of hidden activations → single scalar
					;   sigmoid(...): squash to (0,1) range — the network's output
					;   probability
		    (sigmoid (dot W2 h1)))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define predict (lambda (input)
		  (if (< 0.5 (forward input)) 1 0)))

(define train (lambda (n)
		(if (< 0 n)
		    (let*
		      (_ (train-epoch 0))
		      ; gc between epochs: frees temporary tensors from the epoch
		      ; before the tail call. safe here because n is a plain number,
		      ; already read, and the recursive call does not need any of
		      ; the let* locals.
		      (_ (gc))
		      (train (- n 1))
		      )
		    ())
		))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(train 1000)
; (print W1)
; (print W2)

(print (make-tensor (predict [0 0])
             (predict [0 1])
             (predict [1 0])
             (predict [1 1])))

;; matrix_test.lisp — examples of matrix literals and operations
;; Run any expression with:
;;   printf '<expr>\n' | ./build/Darwin/arm64/basis.debug

					; ---- matrix literals ----
(print [[1 2 3] [4 5 6]])
					; => [[1 2 3] [4 5 6]]

					; ---- shape and rank ----
(print (shape [[1 2 3] [4 5 6]]))
					; => [2 3]

(print (rank [[1 2 3] [4 5 6]]))
					; => 2

					; ---- slice a row ----
(print (slice [[10 20 30] [40 50 60]] 0))
					; => [10 20 30]

(print (slice [[10 20 30] [40 50 60]] 1))
					; => [40 50 60]

					; ---- element-wise arithmetic on matrices ----
(print (+ [[1 2] [3 4]] [[10 20] [30 40]]))
					; => [[11 22] [33 44]]

(print (* [[1 2] [3 4]] [[2 2] [2 2]]))
					; => [[2 4] [6 8]]

(print (- [[10 20] [30 40]] [[1 2] [3 4]]))
					; => [[9 18] [27 36]]

					; ---- scalar broadcast on matrices ----
(print (* [[1 2] [3 4]] 10))
					; => [[10 20] [30 40]]

(print (+ [[1 2] [3 4]] 1))
					; => [[2 3] [4 5]]

					; ---- matmul: square matrices ----
(print (@ [ [1 2]
	    [3 4] ]

	  [ [5 6]
	    [7 8] ] ))
					; => [[19 22] [43 50]]

					; ---- matmul: rectangular ----
(print (@ [ [1 2 3]
	    [4 5 6] ]
	    
	  [ [7 8]
	    [9 10]
	    [11 12] ]))
					; => [[58 64] [139 154]]

					; ---- matmul: matrix * vector ----
(print (@ [[1 0] [0 1]] [3 7]))
					; => [3 7]   (identity matrix leaves vector unchanged)

(print (@ [[2 0] [0 3]] [4 5]))
					; => [8 15]

					; ---- matmul spelled out ----
(print (matmul [[1 2] [3 4]] [[1 0] [0 1]]))
					; => [[1 2] [3 4]]   (multiply by identity)

					; ---- chained matmul ----
(print (@ (@ [[1 2] [3 4]] [[5 6] [7 8]]) [[1 0] [0 1]]))
					; => [[19 22] [43 50]]   (A*B*I == A*B)

					; ---- define a matrix and reuse it ----
(define A [[1 2] [3 4]])
(define B [[5 0] [0 5]])
(print (@ A B))
					; => [[5 10] [15 20]]   (scalar-matrix multiply via matmul)

					; ---- transpose ----
(print (T A))
					; => [[1 3] [2 4]]

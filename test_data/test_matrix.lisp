;; matrix_test.lisp — examples of matrix literals and operations
;; Run any expression with:
;;   ./build/Darwin/arm64/basis.debug -f ./test_data/matrix_test.lisp

(load "./test_data/mod_testing.lisp")

					; ---- matrix literals ----
(print [[1 2 3] [4 5 6]])
					; => [[1 2 3] [4 5 6]]

(print "-- shape and rank --")
(assert (shape [[1 2 3] [4 5 6]]) [2 3])
					; => [2 3]

(assert (rank [[1 2 3] [4 5 6]]) 2)
					; => 2

(print "-- slice a row --")
(assert (slice [[10 20 30] [40 50 60]] 0) [10 20 30] )
					; => [10 20 30]

(assert (slice [[10 20 30] [40 50 60]] 1) [40 50 60])
					; => [40 50 60]

(print "-- element-wise arithmetic on matrices --")
(assert (+ [[1 2] [3 4]] [[10 20] [30 40]]) [[11 22] [33 44]])
					; => [[11 22] [33 44]]

(assert (* [[1 2] [3 4]] [[2 2] [2 2]]) [[2 4] [6 8]])
					; => [[2 4] [6 8]]

(assert (- [[10 20] [30 40]] [[1 2] [3 4]]) [[9 18] [27 36]])
					; => [[9 18] [27 36]]

(print "-- scalar broadcast on matrices --")
(assert (* [[1 2] [3 4]] 10) [[10 20] [30 40]])
					; => [[10 20] [30 40]]

(assert (+ [[1 2] [3 4]] 1) [[2 3] [4 5]])
					; => [[2 3] [4 5]]

(print "-- matmul: square matrices --")
(assert (@ [ [1 2]
	    [3 4] ]

	  [ [5 6]
	  [7 8] ] )

	[[19 22] [43 50]]
	)
					; => [[19 22] [43 50]]

(print "-- matmul: rectangular --")
(assert (@ [ [1 2 3]
	    [4 5 6] ]
	    
	  [ [7 8]
	    [9 10]
	    [11 12] ])

	[[58 64] [139 154]]
	)
					; => [[58 64] [139 154]]

(print "-- matmul: matrix * vector --")
(assert (@ [[1 0] [0 1]] [3 7]) [3 7])
					; => [3 7]   (identity matrix leaves vector unchanged)

(assert (@ [[2 0] [0 3]] [4 5]) [8 15])
					; => [8 15]

(print "-- matmul spelled out --")
(assert (matmul [[1 2] [3 4]]
		[[1 0] [0 1]])
	
	        [[1 2] [3 4]]
	)
					; => [[1 2] [3 4]]   (multiply by identity)

(print "-- chained matmul --")
(assert (@
	 (@ [[1 2] [3 4]]
	    [[5 6] [7 8]])
	 
	    [[1 0] [0 1]]
	) [[19 22] [43 50]])
					; => [[19 22] [43 50]]   (A*B*I == A*B)

(print "-- define a matrix and reuse it --")
(define A [[1 2] [3 4]])
(define B [[5 0] [0 5]])
(assert (@ A B) [[5 10] [15 20]])
					; => [[5 10] [15 20]]   (scalar-matrix multiply via matmul)

(print "-- transpose --")
(assert (T A) [[1 3] [2 4]])
					; => [[1 3] [2 4]]

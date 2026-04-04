; projection_test.lisp
; 3D camera matrix demo: view matrix and perspective projection.
;
; The key idea: matrix entries are live s-expressions. (cos yaw) and
; (sin pitch) are not pre-computed constants pasted in - they are
; evaluated fresh each time the lambda is called, so changing the
; angle argument changes the matrix automatically.
;
; Run with: ./build/Darwin/arm64/basis.debug -f test_data/projection_test.lisp

(define pi 3.14159265358979)
(define deg->rad (lambda (d) (* d (/ pi 180))))
(define tan     (lambda (x) (/ (sin x) (cos x))))

					; ---- Rotation matrices ----
					; The (cos yaw) / (sin yaw) calls inside [ ] are
					; s-expressions that get evaluated when the lambda
					; runs - they are the matrix entries, not macros.

(define rot-y (lambda (yaw)
    [[(cos yaw)          0  (sin yaw)          0]
     [0                  1  0                  0]
     [(* -1 (sin yaw))   0  (cos yaw)          0]
     [0                  0  0                  1]]))

(define rot-x (lambda (pitch)
    [[1  0              0                   0]
     [0  (cos pitch)    (* -1 (sin pitch))  0]
     [0  (sin pitch)    (cos pitch)         0]
     [0  0              0                   1]]))

					; ---- Translation matrix ----

(define translate (lambda (tx ty tz)
    [[1  0  0  tx]
     [0  1  0  ty]
     [0  0  1  tz]
     [0  0  0  1]]))

					; ---- View matrix ----
					; Places the camera at world position (cx cy cz) looking with
					; yaw and pitch. Equivalent to: rot-x(pitch) @ rot-y(yaw) @
					; translate(-eye) This is the standard game-engine view
					; construction.

(define make-view (lambda (cx cy cz yaw pitch)
    (@ (rot-x pitch)
       (@ (rot-y yaw)
          (translate (* -1 cx) (* -1 cy) (* -1 cz))))))

					; ---- Perspective projection matrix ----
					; Standard OpenGL-style perspective matrix.
					; fov:    vertical field of view in radians
					; aspect: viewport width / height
					; near:   near clip plane distance
					; far:    far clip plane distance
					;
					; f = 1 / tan(fov/2) lives as a local binding via let* so the
					; matrix entries (/ f aspect) etc. are s-expressions over f.

(define make-perspective (lambda (fov aspect near far)
    (let* (f (/ 1 (tan (/ fov 2))))
    [[(/ f aspect)  0  0                                  0                                      ]
     [0             f  0                                  0                                      ]
     [0             0  (/ (+ far near) (- near far))      (/ (* 2 (* far near)) (- near far))    ]
     [0             0  -1                                 0                                      ]])))

					; ---- Demo ----

					; Camera at (0 0 5) looking straight forward - rotation is
					; identity so view should be
					; [[1 0 0 0][0 1 0 0][0 0 1 -5][0 0 0 1]]
(define view (make-view 0 0 5 0 0))
(print view)

					; 60 degree vertical FOV, 16:9 aspect, near=0.1, far=100
					; f = 1/tan(30 deg) ~= 1.732
(define proj (make-perspective (deg->rad 60) (/ 16 9) 0.1 100))
(print proj)

					; Full camera matrix: projection @ view
(define camera (@ proj view))
(print camera)

					; Camera rotated 45 degrees left (yaw) and 30 degrees up
					; (pitch)
(define view2 (make-view 0 0 5 (deg->rad 45) (deg->rad 30)))
(define camera2 (@ proj view2))
(print camera2)

					; Transform world points through the camera (homogeneous
					; coords) mat @ vec gives clip-space position before
					; perspective divide

					; Origin [0 0 0 1] - camera is at z=5 so this is 5 units in front
(print (@ camera [0 0 0 1]))

					; Point one unit in front of the camera [0 0 4 1]
(print (@ camera [0 0 4 1]))

					; Same point through the rotated camera
(print (@ camera2 [0 0 4 1]))

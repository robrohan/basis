;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; test_kb.lisp - tests for mod_kb.lisp

(load "test_data/mod_testing.lisp")
(load "test_data/mod_kb.lisp")

					; build the KB with define - each define re-anchors env
					; below the new cons cells so gc_core preserves them.
					; set! does not update env and the new list cells get
					; freed on the next gc. Tensors are fine with
					; set! because they live in tensor_heap, not the cons
					; cell stack.
(define kb '((dog is-a animal)
             (cat is-a animal)
             (animal is-a living-thing)
             (dog has fur)
             (dog has legs)
	     (car bbox [5 4 3])
             (car mass 1500)
             (dog mass 30)))

					; what things are is-a animal?
(print (kb-query '(?x is-a animal) kb))

					; what does dog have?
(print (kb-get 'dog 'has kb))

					; get a tensor property
(print (kb-get 'car 'bbox kb))

					; transitive inference: what is dog?
(print (kb-infer 'dog 'is-a kb))

					; retract fur and verify only legs remains
(define kb (kb-retract '(dog has fur) kb))
(print (kb-get 'dog 'has kb))

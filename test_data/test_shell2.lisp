#!/usr/bin/env basis

(load "test_data/mod_testing.lisp")
(load "test_data/mod_kb.lisp")


(define kb '(
	     (matter is-a foundation)       ; usually denotes stuff with particle-like, localized properties
	     (energy is-a foundation)       ; energy is a broader scalar conserved quantity in fields, radiation, motion, etc
	     (space is-a foundation)        ; 
	     (time is-a foundation)         ; space-time?
	     
             ;;;;;;;;;;;;;;;;;;;;;;;

	     (thing is-a matter)
	     (living-thing is-a thing)
	     (living-thing has life)
	     
             ;;;;;;;;;;;;;;;;;;;;;;;

	     (planet is-a thing)
	     (earth is-a planet)
	     (earth is-a place)
	     (earth mass 5.9722E+24)        ; kg - 9.8m/s^2 property?
	     (earth radius [6378000])       ; meters
	     (earth body fixed)

	     ;;;;;;;;;;;;;;;;;;;;;;;
	     	     
	     (animal is-a living-thing)
	     (animal exists-on earth)

	     ;;;;;;;;;;;;;;;;;;;;;;;
	     
	     (dog is-a animal)
	     (dog has fur)
	     
	     (human is-a animal)
	     (human bbox [0.4 0.7 2.0])
	     (human body rigid)

	     (cat is-a animal)
	     (cat has fur)                  ; Random fact about cat
	     (cat has whiskers)             ; Random fact about cat
	     (cat avg_mass 4.5)             ; in KG
	     (cat bbox [0.5 0.25 0.35])     ; meters
	     (cat body rigid)
	     
	     ;;;;;;;;;;;;;;;;;;;;;;;

	     (mountain is-a thing)
	     (mountain exists-on earth)
	     
	     ))


(print (kb-query '(?x is-a foundation) kb))


(print "❓")				; what things are is-a animal?
(print (kb-query '(?x is-a animal) kb))


(print "🐈")				; what does a cat have?
(print (kb-get 'cat 'has kb))


(print "🐈📦")				; get a tensor property
(print (kb-get 'cat 'bbox kb))


					
(print "🐈->🌏")			; transitive inference: what is cat?
(print (kb-infer 'cat 'is-a kb))


					; retract fur and verify only legs remains
; (define kb (kb-retract '(cat has fur) kb))
(print (kb-get 'cat 'has kb))

(print (kb-get 'earth 'mass kb))


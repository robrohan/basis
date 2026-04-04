;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; mod_kb.lisp
;; Knowledge base operations built on top of match.
;;
;; The KB is a flat list of fact triples:
;;   (subject predicate object)
;;
;; Symbolic:  (dog is-a animal)
;; Property:  (car mass 1500)
;; Tensor:    (car bbox [2.0 1.5 4.5])
;;
;; Usage:
;;   (load "test_data/mod_kb.lisp")
;;   (define kb (quote ()))
;;   (set! kb (kb-assert (quote (dog is-a animal)) kb))
;;   (kb-query (quote (?x is-a animal)) kb)
;;
;; Note: kb-infer walks chains transitively but does not detect cycles.

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; List utilities

(define filter (lambda (pred lst)
    (if (not lst)
        ()
        (if (pred (car lst))
            (cons (car lst) (filter pred (cdr lst)))
            (filter pred (cdr lst))))))

(define map (lambda (f lst)
    (if (not lst)
        ()
        (cons (f (car lst)) (map f (cdr lst))))))

(define append (lambda (a b)
    (if (not a)
        b
        (cons (car a) (append (cdr a) b)))))

(define flatten (lambda (lst)
    (if (not lst)
        ()
        (if (not (car lst))
            (flatten (cdr lst))
            (if (pair? (car lst))
                (append (flatten (car lst)) (flatten (cdr lst)))
                (cons (car lst) (flatten (cdr lst))))))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Association list lookup
;; (assoc is internal C only, not exposed as a primitive)

(define assoc (lambda (key alist)
    (if (not alist)
        ()
        (if (eq? (car (car alist)) key)
            (cdr (car alist))
            (assoc key (cdr alist))))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; KB operations

;; kb-assert: prepend a fact, return new KB
(define kb-assert (lambda (fact kb)
    (cons fact kb)))

;; kb-retract: remove all facts matching pattern, return new KB
(define kb-retract (lambda (pattern kb)
    (filter (lambda (f) (eq? (match pattern f) ERR)) kb)))

;; kb-query: return list of binding alists for all matching facts
(define kb-query (lambda (pattern kb)
    (filter (lambda (b) (not (eq? b ERR)))
            (map (lambda (fact) (match pattern fact)) kb))))

;; kb-get: shorthand for (subject predicate ?value)
;; returns a flat list of matching values
(define kb-get (lambda (subject predicate kb)
    (map (lambda (b) (assoc (quote value) b))
         (kb-query (cons subject (cons predicate (cons (quote ?value) ()))) kb))))

;; kb-infer: transitive closure of a relation
;; returns all objects reachable from subject via repeated rel lookups
;; e.g. (kb-infer 'dog 'is-a kb) => (animal living-thing ...)
(define kb-infer (lambda (subject rel kb)
    (let* (direct (kb-get subject rel kb))
    (if (not direct)
        ()
        (append direct
                (flatten (map (lambda (parent) (kb-infer parent rel kb))
                              direct)))))))

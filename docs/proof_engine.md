---
Project: Basis
Date: 2026-04-15
---

# Proof Engine Design

## 1. Motivation

Basis already has a symbolic layer: facts stored as triples in a knowledge base,
and a `match` primitive that does one-directional pattern matching. This is enough
to verify that a property holds computationally up to some bound N. It is not
enough to prove it holds for all N.

The missing step is **unification** — bidirectional pattern matching where variables
can appear on either side. With unification, you can represent proof rules as rewrite
rules and apply them mechanically to transform a goal into something already known
to be true. That is a proof engine.

The goal of this document is to describe:

1. What unification is and how it differs from `match`
2. The Robinson unification algorithm
3. How rewrite rules are represented in Basis
4. How backward-chaining proof search works in Lisp
5. How mathematical induction is encoded
6. What needs to be implemented in C vs Lisp
7. Worked examples

---

## 2. Unification vs Match

`match` in Basis is one-directional. Variables (atoms beginning with `?`) appear
only in the pattern (left side). The data (right side) is ground — no variables.

```lisp
(match '(?x is-a animal) '(dog is-a animal))
; => ((x . dog))

(match '(?x is-a ?y) '(dog is-a animal))
; => ((x . dog) (y . animal))
```

`unify` is bidirectional. Variables can appear on **both** sides. The algorithm
finds a substitution σ such that applying σ to both terms makes them identical.

```lisp
(unify '(?x + 1) '(n + 1))
; => ((x . n))

(unify '(?x + ?y) '(?a + b))
; => ((x . ?a) (y . b))

(unify '(?x + ?x) '(2 + 3))
; => ERR  ; x can't be both 2 and 3
```

The practical difference: `match` lets you check if data fits a pattern.
`unify` lets you find what two expressions have in common, or what substitution
makes them the same. Proof search needs `unify` because you work backwards from
a goal — the goal has variables, and the rule conclusions also have variables.

---

## 3. The Robinson Unification Algorithm

Robinson's 1965 algorithm. Given two terms t1 and t2, find the **most general
unifier** (MGU) — the substitution with the fewest commitments that still makes
t1 and t2 identical.

### 3.1 Terms

In Basis, terms are ordinary S-expressions:
- **Atoms** — symbols like `dog`, `+`, `n`
- **Variables** — atoms starting with `?`, like `?x`, `?n`
- **Compound terms** — lists like `(+ ?x 1)` or `(is-a dog animal)`
- **Numbers** — self-unify only with identical numbers
- **Tensors** — self-unify only with structurally identical tensors

### 3.2 Algorithm (recursive)

```
unify(t1, t2, σ):
  t1 = apply(σ, t1)        ; substitute what we know so far
  t2 = apply(σ, t2)

  if t1 == t2:              ; identical — already unified
      return σ

  if t1 is a variable:
      if occurs(t1, t2):    ; OCCURS CHECK — see 3.3
          return FAIL
      return σ ∪ {t1 → t2}

  if t2 is a variable:
      if occurs(t2, t1):
          return FAIL
      return σ ∪ {t2 → t1}

  if both are compound and same length:
      for each pair (a, b) of corresponding sub-terms:
          σ = unify(a, b, σ)
          if σ == FAIL: return FAIL
      return σ

  return FAIL               ; incompatible structure
```

### 3.3 The Occurs Check

Before binding variable `?x` to term `t`, check that `?x` does not appear
inside `t`. Without this check, you could create circular substitutions like
`?x → (f ?x)`, which has no finite solution.

Example that fails the occurs check:
```lisp
(unify '?x '(f ?x))   ; => ERR — x appears inside (f x)
```

The occurs check makes unification O(n²) in the worst case. Many Prolog
implementations skip it for speed (unsound but fast). Basis includes it by
default.

### 3.4 Applying Substitutions

A substitution σ is an alist of `(variable . term)` pairs — exactly the
same format `match` already returns. Applying σ to a term means walking the
term and replacing every variable found in σ with its bound value, recursively
until no more substitutions apply.

The existing alist machinery from `mod_kb.lisp` reuses directly.

---

## 4. Implementation Plan

### 4.1 C primitive: `unify`

`unify` is performance-sensitive (proof search calls it in a tight loop) and
requires careful recursive structure — implement it as a C primitive in
`tinysymbolic.c` alongside `match`.

Signature: `(unify t1 t2)` → alist of bindings or `ERR`

The implementation mirrors `match_rec` but both sides can be variables, and
the accumulated substitution σ is threaded through the recursion so earlier
bindings are visible when processing later sub-terms.

This is tracked as a backlog ticket.

### 4.2 Lisp layer: substitution application

```lisp
;; apply-subst: walk term t, replace any variable found in bindings
(defun apply-subst (bindings t)
    (if (not t) ()
        (if (atom? t)
            (let* (b (assoc t bindings))
                (if b b t))
            (cons (apply-subst bindings (car t))
                  (apply-subst bindings (cdr t))))))
```

Lives in `test_data/mod_proof.lisp`. Pure Lisp — no C needed.

### 4.3 Lisp layer: rewrite rules

A rewrite rule is a triple `(rule lhs rhs)` meaning "a term that unifies with
`lhs` can be replaced by `rhs` with the same bindings applied."

Rules live as ordinary lists:

```lisp
(define rules
    '((rule (+ ?n 0)             ?n)
      (rule (+ 0 ?n)             ?n)
      (rule (* ?n 1)             ?n)
      (rule (* 1 ?n)             ?n)
      (rule (* ?n 0)             0)
      (rule (+ ?n (+ ?m ?p))    (+ (+ ?n ?m) ?p))))
```

### 4.4 Lisp layer: single-step rewriting

```lisp
;; try to rewrite term t using one rule, return new term or ()
(defun rewrite-once (rules t)
    (if (not rules) ()
        (let* (rule  (car rules)
               lhs   (car (cdr rule))
               rhs   (car (cdr (cdr rule)))
               binds (unify lhs t))
            (if (equal binds ERR)
                (rewrite-once (cdr rules) t)
                (apply-subst binds rhs)))))
```

### 4.5 Lisp layer: proof search (backward chaining)

Given a goal, try to prove it by repeatedly applying rules until the goal
reduces to something in the axiom set.

```lisp
;; prove: attempt to reduce goal to a known truth
;; axioms: list of ground facts known to be true
;; rules:  list of (rule lhs rhs) rewrite rules
;; depth:  recursion limit to prevent infinite loops
(defun prove (goal axioms rules depth)
    (if (= depth 0) ERR
        (if (member goal axioms) #t
            (let* (rewritten (rewrite-once rules goal))
                (if (not rewritten) ERR
                    (prove rewritten axioms rules (- depth 1)))))))
```

---

## 5. Mathematical Induction

Induction is a proof strategy, not a primitive. It is encoded as a rule schema:

```
If P(0) is true, and
   for all ?n: P(?n) → P(?n + 1),
Then P(?n) is true for all natural ?n.
```

In Basis:

```lisp
;; Step 1: assert the base case as an axiom
(define axioms
    (kb-assert '(sum-formula holds-for 0) '()))

;; Step 2: assert the inductive step as a rule
;; "if (sum-formula holds-for ?n) then (sum-formula holds-for (+ ?n 1))"
(define rules
    '((rule (sum-formula holds-for (+ ?n 1))
            (sum-formula holds-for ?n))))

;; Step 3: ask the proof engine to prove a specific instance
(prove '(sum-formula holds-for 5) axioms rules 10)
```

The engine would:
1. Check axioms — `(sum-formula holds-for 5)` is not there
2. Unify the goal with the rule lhs: `?n = 4`, produce subgoal `(sum-formula holds-for 4)`
3. Recurse: `4` → `3` → `2` → `1` → `0`
4. `(sum-formula holds-for 0)` is in axioms — return `#t`

This does not simplify `n*(n+1)/2 + (n+1)` to `(n+1)*(n+2)/2` symbolically.
For that, algebraic rewrite rules covering commutativity, distributivity, and
factoring are needed in the rule set. Those rules can be added incrementally
in Lisp without any C changes.

---

## 6. What Goes Where

| Component         | Where              | Why                                              |
|-------------------|--------------------|--------------------------------------------------|
| `unify`           | C primitive        | called in every proof step, performance matters  |
| `apply-subst`     | `mod_proof.lisp`   | simple recursive walk                            |
| `rewrite-once`    | `mod_proof.lisp`   | rule lookup + substitution application           |
| `prove`           | `mod_proof.lisp`   | search strategy, easy to swap out or extend      |
| Arithmetic rules  | user Lisp          | data, not code — user extends as needed          |
| Induction schema  | user Lisp          | same                                             |
| Algebraic rules   | user Lisp          | add incrementally                                |

---

## 7. Limitations and Future Work

- **No symbolic algebra** — the rule set must cover all algebraic steps manually.
  A CAS-style simplifier would remove this burden but is far out of scope.
- **No cycle detection** — the depth limit prevents infinite loops but is a blunt
  instrument. A visited-set would be cleaner.
- **No proof terms** — `prove` returns `#t` or `ERR`, not a machine-checkable
  proof object. Adding a trace of which rules fired is a natural next step.
- **Depends on `unify`** — nothing here works until the C `unify` primitive
  exists. `match` cannot substitute for it.

---

## 8. Worked Example: Commutativity of Addition

```lisp
(load "test_data/mod_kb.lisp")
(load "test_data/mod_proof.lisp")

(define axioms '((comm-add holds)))

(define rules
    '((rule (comm-add holds)  (comm-add holds))   ; axiom is self-evident
      (rule (+ ?a ?b)         (+ ?b ?a))))        ; commutativity

;; show that (+ x y) and (+ y x) unify — i.e. they are the same up to variable renaming
(unify '(+ ?a ?b) '(+ y x))
; => ((a . y) (b . x))
```

A real equality proof needs an explicit equality goal type rather than just
reachability. The design leaves that open so the Lisp layer can evolve without
C changes.

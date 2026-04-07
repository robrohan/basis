#!/usr/bin/env basis
; cust_blocksworld.lisp — blocksworld simulation using the KB
;
; Demonstrates physical constraint checking: a block can only be moved
; if it is "clear" (nothing resting on top of it).
;
; This catches the classic LLM reasoning error:
;   "Put C on A" appears to succeed, but C has B on top of it —
;   the KB enforces the constraint and returns ERR on illegal moves.
;
; Initial stack (bottom to top): C (green), B (blue), A (red)
; Instructions: move A to table, then put C on A
; Naive answer: top=green, bottom=red
; Correct answer: must move B first; B ends up on table
;                 final stack: A (red, bottom), C (green, top)
;                 B (blue) is on the table separately
;
; ── Background / related work ──────────────────────────────────────────────
;
; The Blocksworld domain originated in classical AI planning:
;
;   Winograd, T. (1972). "Understanding Natural Language."
;   Academic Press. — SHRDLU system; one of the first programs to
;   manipulate blocks via natural language with genuine constraint checking.
;   The "clear" precondition appears explicitly here.
;
;   Fikes, R. & Nilsson, N. (1971). "STRIPS: A New Approach to the
;   Application of Theorem Proving to Problem Solving."
;   Artificial Intelligence, 2(3-4), 189-208.
;   — Formalised planning with preconditions/effects; blocksworld was
;   the canonical benchmark domain.
;
; LLM failure on blocksworld / physical state tracking:
;
;   Valmeekam et al. (2022). "Large Language Models Still Can't Plan
;   (A Benchmark for LLMs on Classical Planning)."
;   NeurIPS 2022 Workshop on Thinking for Doing.
;   arXiv:2206.10498
;   — Shows GPT-3/4 fail on blocksworld planning tasks that STRIPS solvers
;   handle trivially; failure rate increases sharply with plan length.
;
;   Valmeekam et al. (2023). "PlanBench: An Extensible Benchmark for
;   Evaluating Large Language Models on Planning and Reasoning about Change."
;   NeurIPS 2023. arXiv:2206.10498 (updated)
;   — Follow-up with more rigorous evaluation; LLMs still struggle with
;   multi-step state tracking even with chain-of-thought prompting.
;
;   Bubeck et al. (2023). "Sparks of Artificial General Intelligence:
;   Early experiments with GPT-4." arXiv:2303.12528
;   — Section on planning includes blocksworld; notes GPT-4 can handle
;   simple cases but degrades on longer sequences (see §4).
;
; Neuro-symbolic / grounded approaches (relevant to this project):
;
;   Mao et al. (2019). "The Neuro-Symbolic Concept Learner: Interpreting
;   Scenes, Words, and Sentences From Natural Supervision."
;   ICLR 2019. arXiv:1904.12584
;   — Learns symbolic KB facts from perception; separates symbol grounding
;   from reasoning. Close in spirit to what we're doing with bbox/mass facts.
;
;   Garcez & Lamb (2023). "Neurosymbolic AI: The 3rd Wave."
;   Artificial Intelligence Review. arXiv:2012.05876
;   — Survey covering the spectrum from pure neural to pure symbolic;
;   useful framing for where this project sits.
;
;   Liang et al. (2023). "Code as Policies: Language Model Programs for
;   Embodied Control." ICRA 2023. arXiv:2209.07753
;   — Uses LLMs to emit executable code (rather than natural language plans)
;   for robot control; the KB+basis approach here is a minimal version of
;   the same idea — ground truth lives in the symbolic layer, not the LLM.
;
; NOTE: paper details are from training data (cutoff Aug 2025) — verify
; arXiv IDs and publication venues before citing.

(load "test_data/mod_kb.lisp")

;; ── State ──────────────────────────────────────────────────────────────────

(define state '(
  (A color red)
  (B color blue)
  (C color green)
  (A on B)        ; A rests directly on B
  (B on C)        ; B rests directly on C
  (C on table)    ; C is on the table
))

;; ── Helpers ────────────────────────────────────────────────────────────────

;; Build the pattern (?who on X) at runtime so X can be a variable
(define above-pattern (lambda (x)
  (cons '?who (cons 'on (cons x ())))))

;; A block is clear if nothing is resting on top of it
(define clear? (lambda (x s)
  (not (kb-query (above-pattern x) s))))

;; What is block X currently resting on?
(define resting-on (lambda (x s)
  (car (kb-get x 'on s))))

;; Move block X to target — enforces clear? constraint, returns ERR if blocked
(define move-to (lambda (x target s)
  (if (clear? x s)
    (let* (cur (resting-on x s))
      (let* (s2 (kb-retract (cons x (cons 'on (cons cur ()))) s))
        (kb-assert (cons x (cons 'on (cons target ()))) s2)))
    ERR)))

(define color-of (lambda (x s)
  (car (kb-get x 'color s))))

;; ── Simulation ─────────────────────────────────────────────────────────────

;; Step 1: Move A to table (A is clear — legal)
(define state (move-to 'A 'table state))
(print "after moving A to table:")
(print (resting-on 'A state))           ; table

;; Step 2: Try to put C on A — illegal, B is on top of C
(print "attempt to move C directly onto A:")
(print (move-to 'C 'A state))           ; ERR

;; Step 3: B is now the top of the remaining stack — move it first
(define state (move-to 'B 'table state))
(print "after moving B to table:")
(print (resting-on 'B state))           ; table

;; Step 4: C is now clear — legal
(define state (move-to 'C 'A state))
(print "after moving C onto A:")
(print (resting-on 'C state))           ; A

;; ── Final state ────────────────────────────────────────────────────────────

(print "--- final stack (bottom to top) ---")
(print (color-of 'A state))             ; red   (bottom)
(print (color-of 'C state))             ; green (top)
(print "B is on:")
(print (resting-on 'B state))           ; table (separate)

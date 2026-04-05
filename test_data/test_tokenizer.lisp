;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; test_tokenizer.lisp
;; Demonstrates load-gguf-vocab, tokenize, detokenize, and token->str.
;;
;; Run: basis -f test_data/test_tokenizer.lisp
;;
;; Requires: models/gpt2.Q4_0.gguf  (make download_gpt2)

(load-gguf-vocab "models/gpt2.Q4_0.gguf")
(print "vocab loaded")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; token->str: single ID -> decoded string

(print "--- single token decode ---")
(print (token->str 15496))   ; Hello
(print (token->str 11))      ; ,
(print (token->str 314))     ;  I
(print (token->str 716))     ;  am
(print (token->str 50256))   ; <|endoftext|>

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; tokenize: text -> rank-1 tensor of IDs

(print "--- tokenize ---")
(define toks (tokenize "Hello, I am"))
(print (shape toks))   ; [4]
(print toks)           ; [15496 11 314 716]

(define toks2 (tokenize "once upon a time"))
(print (shape toks2))
(print toks2)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; detokenize: rank-1 tensor -> reconstructed string

(print "--- detokenize round-trip ---")
(print (detokenize toks))    ; Hello, I am
(print (detokenize toks2))   ; once upon a time

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; byte-level round-trip: punctuation and spaces

(print "--- edge cases ---")
(print (detokenize (tokenize "The quick brown fox.")))
(print (detokenize (tokenize "1 + 1 = 2")))

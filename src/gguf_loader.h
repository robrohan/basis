#ifndef GGUF_LOADER_H
#define GGUF_LOADER_H

/* (load-gguf "model.gguf") — open a GGUF file, dequantize every tensor to
   float32, allocate each in the basis tensor heap, and bind it by name in
   the global env.  Returns the number of tensors loaded, or ERR on failure.

   After loading, tensor names are available as atoms:
     token_embd.weight        ; [n_vocab x n_embd]
     blk.0.attn_q.weight      ; etc.

   Note: GGUF stores dimensions innermost-first (column-major for 2-D tensors).
   Use (T x) to transpose if your matmul expects row-major weight matrices. */

void register_gguf_prims(void);

#endif /* GGUF_LOADER_H */

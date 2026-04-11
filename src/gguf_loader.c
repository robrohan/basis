#include "tinylisp.h"
#include "tinytensor.h"
#include "gguf_loader.h"
#include "gguflib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* (load-gguf "model.gguf")
   Opens the GGUF file, skips metadata, iterates every tensor, dequantizes
   each to float32, allocates it in the basis tensor heap, and binds the
   tensor name as an atom in the global s->l_env.
   Returns the count of loaded tensors, or L_ERR if the file cannot be opened. */
static L f_load_gguf(lisp_state_t *s, L t, L e)
{
    L arg = car(s, evlis(s, t, e));
    if (T(arg) != STR && T(arg) != ATOM) return s->l_err;

    const char *path = A(s) + ord(arg);

    gguf_ctx *ctx = gguf_open(path);
    if (!ctx) {
        fprintf(stderr, "load-gguf: cannot open '%s'\n", path);
        return s->l_err;
    }

    /* skip all metadata key-value pairs — we only want tensor weights */
    gguf_skip_key_values_section(ctx);

    int loaded = 0;
    gguf_tensor tensor;

    while (gguf_get_tensor(ctx, &tensor)) {
        /* dequantize to a fresh malloc'd float32 array */
        float *data = gguf_tensor_to_float(&tensor);
        if (!data) {
            fprintf(stderr, "load-gguf: skipping '%.*s' (unsupported quant type %u)\n",
                    (int)tensor.namelen, tensor.name, tensor.type);
            continue;
        }

        /* build basis shape from GGUF dims (stored innermost-first).
           GGUF 2D tensors: dim[0] is the inner (fastest-varying) axis,
           dim[1] is the outer (slowest-varying) axis.  The data layout is
           therefore (dim[1] rows) × (dim[0] cols) in row-major — i.e. the
           PyTorch weight shape, not its transpose.  We swap the indices so
           basis stores shape[0]=rows=dim[1], shape[1]=cols=dim[0], making
           basis's row-major indexing match the actual data layout.
           1D tensors (biases, norms) are not affected. */

        II rank  = (II)tensor.ndim;
        II len   = (II)tensor.num_weights;
        II shape[MAX_RANK];
        II i;
        if (rank == 2) {
            shape[0] = (II)tensor.dim[1];   /* outer dim → rows */
            shape[1] = (II)tensor.dim[0];   /* inner dim → cols */
        } else {
            for (i = 0; i < rank; i++)
                shape[i] = (II)tensor.dim[i];
        }

        /* allocate tensor in basis heap, then free the temporary float array */
        tensor_t *bt = alloc_tensor(s, rank, shape, len, data);
        free(data);

        /* null-terminate the GGUF name (not null-terminated in the file) */
        char name[256];
        size_t nl = tensor.namelen < 255 ? tensor.namelen : 255;
        memcpy(name, tensor.name, nl);
        name[nl] = '\0';

        /* bind name → tensor in the global s->l_environment */
        s->l_env = pair(s, atom(s, name), box(TENS, (II)(bt - s->tensor_heap)), s->l_env);

        fprintf(stderr, "  %s  [", name);
        for (i = 0; i < rank; i++) {
            if (i) fprintf(stderr, " x ");
            fprintf(stderr, "%u", shape[i]);
        }
        fprintf(stderr, "]\n");

        loaded++;
    }

    gguf_close(ctx);
    fprintf(stderr, "load-gguf: %d tensors loaded from '%s'\n", loaded, path);
    return (L)(double)loaded;
}

void register_gguf_prims(lisp_state_t *s)
{
    register_prim(s, "load-gguf", f_load_gguf);
}

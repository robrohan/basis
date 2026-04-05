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
   tensor name as an atom in the global env.
   Returns the count of loaded tensors, or ERR if the file cannot be opened. */
static L f_load_gguf(L t, L e)
{
    L arg = car(evlis(t, e));
    if (T(arg) != STR && T(arg) != ATOM) return err;

    const char *path = A + ord(arg);

    gguf_ctx *ctx = gguf_open(path);
    if (!ctx) {
        fprintf(stderr, "load-gguf: cannot open '%s'\n", path);
        return err;
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

        /* build basis shape from GGUF dims (stored innermost-first) */
        I rank  = (I)tensor.ndim;
        I len   = (I)tensor.num_weights;
        I shape[MAX_RANK];
        I i;
        for (i = 0; i < rank; i++)
            shape[i] = (I)tensor.dim[i];

        /* allocate tensor in basis heap, then free the temporary float array */
        tensor_t *bt = alloc_tensor(rank, shape, len, data);
        free(data);

        /* null-terminate the GGUF name (not null-terminated in the file) */
        char name[256];
        size_t nl = tensor.namelen < 255 ? tensor.namelen : 255;
        memcpy(name, tensor.name, nl);
        name[nl] = '\0';

        /* bind name → tensor in the global environment */
        env = pair(atom(name), box(TENS, (I)(bt - tensor_heap)), env);

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

void register_gguf_prims(void)
{
    register_prim("load-gguf", f_load_gguf);
}

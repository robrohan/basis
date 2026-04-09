#include "tinylisp.h"
#include "tinytensor.h"
#include "tokenizer.h"
#include "gguflib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
 * GPT-2 / tiktoken byte-to-unicode table
 *
 * GPT-2's tokenizer maps every raw byte to a printable unicode
 * codepoint so the vocabulary never contains control characters.
 * Printable ASCII (33-126) and latin-1 supplement (161-172,
 * 174-255) are left as-is; the remaining 68 bytes (0-32, 127-160,
 * 173) are mapped to U+0100..U+0143.  The same convention is used
 * by Qwen and many other GGUF-packaged models.
 * ============================================================ */

static uint32_t b2u[256];   /* byte  -> unicode codepoint */
static uint8_t  u2b[512];   /* codepoint -> byte  (valid for cp < 512) */
static int b2u_ready = 0;

static void init_b2u(void)
{
    int b;
    if (b2u_ready) return;
    int mapped[256] = {0};
    for (b = 33;  b <= 126; b++) { b2u[b] = (uint32_t)b; mapped[b] = 1; }
    for (b = 161; b <= 172; b++) { b2u[b] = (uint32_t)b; mapped[b] = 1; }
    for (b = 174; b <= 255; b++) { b2u[b] = (uint32_t)b; mapped[b] = 1; }
    uint32_t cp = 256;
    for (b = 0; b < 256; b++)
        if (!mapped[b]) b2u[b] = cp++;
    /* cp now == 324 (256 base + 68 remapped bytes) */
    memset(u2b, 0, sizeof(u2b));
    for (b = 0; b < 256; b++)
        if (b2u[b] < 512) u2b[b2u[b]] = (uint8_t)b;
    b2u_ready = 1;
}

/* ============================================================
 * Minimal UTF-8 helpers
 * ============================================================ */

static int utf8_decode(const uint8_t *s, uint32_t *cp)
{
    if (s[0] < 0x80)         { *cp = s[0]; return 1; }
    if ((s[0]&0xE0) == 0xC0) { *cp = ((s[0]&0x1F)<<6)|(s[1]&0x3F); return 2; }
    if ((s[0]&0xF0) == 0xE0) { *cp = ((s[0]&0x0F)<<12)|((s[1]&0x3F)<<6)|(s[2]&0x3F); return 3; }
    *cp = ((s[0]&0x07)<<18)|((s[1]&0x3F)<<12)|((s[2]&0x3F)<<6)|(s[3]&0x3F);
    return 4;
}

static int utf8_encode(uint32_t cp, char *out)
{
    if (cp < 0x80)    { out[0] = (char)cp; return 1; }
    if (cp < 0x800)   { out[0]=(char)(0xC0|(cp>>6)); out[1]=(char)(0x80|(cp&0x3F)); return 2; }
    if (cp < 0x10000) { out[0]=(char)(0xE0|(cp>>12)); out[1]=(char)(0x80|((cp>>6)&0x3F)); out[2]=(char)(0x80|(cp&0x3F)); return 3; }
    out[0]=(char)(0xF0|(cp>>18)); out[1]=(char)(0x80|((cp>>12)&0x3F));
    out[2]=(char)(0x80|((cp>>6)&0x3F)); out[3]=(char)(0x80|(cp&0x3F)); return 4;
}

/* Decode a GPT-2-encoded token string (byte-to-unicode convention) to
 * actual bytes.  out must have room for at least 4*inlen+1 bytes. */
static int tok_decode(const char *tok, char *out)
{
    const uint8_t *p = (const uint8_t *)tok;
    int pos = 0;
    while (*p) {
        uint32_t cp;
        p += utf8_decode(p, &cp);
        if (cp < 512)
            out[pos++] = (char)u2b[cp];
        else {
            /* codepoint beyond the 256-byte range: emit UTF-8 as raw bytes */
            pos += utf8_encode(cp, out + pos);
        }
    }
    out[pos] = '\0';
    return pos;
}

/* ============================================================
 * Vocabulary storage
 * ============================================================ */

static char    **vocab    = NULL;
static uint32_t  vocab_sz = 0;

/* ============================================================
 * Reverse-vocab hash table: token string -> ID
 * Open addressing, power-of-2 size.
 * ============================================================ */

#define RV_SZ (1u << 17)   /* 131072 — enough for up to ~100K tokens */
typedef struct { const char *key; uint32_t id; } rv_entry_t;
static rv_entry_t *rvocab = NULL;

static uint32_t fnv1a(const char *s)
{
    uint32_t h = 2166136261u;
    for (; *s; s++) { h ^= (uint8_t)*s; h *= 16777619u; }
    return h;
}

static void rv_insert(const char *key, uint32_t id)
{
    uint32_t h = fnv1a(key) & (RV_SZ - 1);
    while (rvocab[h].key) h = (h + 1) & (RV_SZ - 1);
    rvocab[h].key = key;
    rvocab[h].id  = id;
}

static int rv_lookup(const char *key, uint32_t *out)
{
    uint32_t h = fnv1a(key) & (RV_SZ - 1);
    while (rvocab[h].key) {
        if (strcmp(rvocab[h].key, key) == 0) { *out = rvocab[h].id; return 1; }
        h = (h + 1) & (RV_SZ - 1);
    }
    return 0;
}

/* ============================================================
 * BPE merge hash table: (left_id, right_id) -> merge rank
 * Lower rank = higher priority merge.
 * ============================================================ */

#define MR_SZ (1u << 17)
typedef struct { uint32_t left, right, rank; int used; } mr_entry_t;
static mr_entry_t *merge_ht = NULL;

static void mr_insert(uint32_t left, uint32_t right, uint32_t rank)
{
    uint32_t h = (left * 2654435761u ^ right * 2246822519u) & (MR_SZ - 1);
    while (merge_ht[h].used) {
        if (merge_ht[h].left == left && merge_ht[h].right == right) return;
        h = (h + 1) & (MR_SZ - 1);
    }
    merge_ht[h].left = left; merge_ht[h].right = right;
    merge_ht[h].rank = rank; merge_ht[h].used  = 1;
}

static int mr_lookup(uint32_t left, uint32_t right, uint32_t *rank)
{
    uint32_t h = (left * 2654435761u ^ right * 2246822519u) & (MR_SZ - 1);
    while (merge_ht[h].used) {
        if (merge_ht[h].left == left && merge_ht[h].right == right) {
            *rank = merge_ht[h].rank; return 1;
        }
        h = (h + 1) & (MR_SZ - 1);
    }
    return 0;
}

/* ============================================================
 * GGUF string-array callback
 * Used to collect tokenizer.ggml.tokens and tokenizer.ggml.merges.
 * ============================================================ */

typedef struct {
    char    **strings;
    uint32_t  count;
    uint32_t  capacity;
} str_arr_t;

static void str_arr_cb(void *priv, uint32_t type, union gguf_value *val,
                       uint64_t in_array, uint64_t array_len)
{
    str_arr_t *s = (str_arr_t *)priv;
    (void)in_array;
    if (type == GGUF_VALUE_TYPE_ARRAY_START) {
        s->capacity = (uint32_t)array_len;
        s->strings  = (char **)malloc(array_len * sizeof(char *));
        s->count    = 0;
    } else if (type == GGUF_VALUE_TYPE_STRING && s->strings) {
        if (s->count < s->capacity) {
            uint64_t slen = val->string.len;
            char *copy = (char *)malloc(slen + 1);
            if (copy) {
                memcpy(copy, val->string.string, slen);
                copy[slen] = '\0';
                s->strings[s->count++] = copy;
            }
        }
    }
    (void)array_len;
}

/* ============================================================
 * (load-gguf-vocab "file.gguf")
 *
 * Reads tokenizer.ggml.tokens and tokenizer.ggml.merges from
 * the GGUF metadata.  Builds the reverse-vocab and merge hash
 * tables needed by tokenize.  Returns the vocabulary size, or
 * L_ERR if the file cannot be opened.
 * ============================================================ */

static L f_load_gguf_vocab(L t, L e)
{
    L arg = car(evlis(t, e));
    if (T(arg) != STR && T(arg) != ATOM) return l_err;
    const char *path = A + ord(arg);

    init_b2u();

    gguf_ctx *ctx = gguf_open(path);
    if (!ctx) {
        fprintf(stderr, "load-gguf-vocab: cannot open '%s'\n", path);
        return l_err;
    }

    /* (re-)allocate hash tables */
    if (!rvocab)   rvocab   = (rv_entry_t *)calloc(RV_SZ, sizeof(rv_entry_t));
    else           memset(rvocab,   0, RV_SZ * sizeof(rv_entry_t));
    if (!merge_ht) merge_ht = (mr_entry_t *)calloc(MR_SZ, sizeof(mr_entry_t));
    else           memset(merge_ht, 0, MR_SZ * sizeof(mr_entry_t));

    /* free old vocab */
    if (vocab) {
        uint32_t i;
        for (i = 0; i < vocab_sz; i++) free(vocab[i]);
        free(vocab);
        vocab = NULL; vocab_sz = 0;
    }

    str_arr_t tok_st   = {0};
    str_arr_t merge_st = {0};

    gguf_key key;
    while (gguf_get_key(ctx, &key)) {
        char kname[128];
        size_t kl = key.namelen < 127 ? key.namelen : 127;
        memcpy(kname, key.name, kl);
        kname[kl] = '\0';

        if (strcmp(kname, "tokenizer.ggml.tokens") == 0)
            gguf_do_with_value(ctx, key.type, key.val, &tok_st,   0, 0, str_arr_cb);
        else if (strcmp(kname, "tokenizer.ggml.merges") == 0)
            gguf_do_with_value(ctx, key.type, key.val, &merge_st, 0, 0, str_arr_cb);
        else
            gguf_do_with_value(ctx, key.type, key.val, NULL, 0, 0, NULL);
    }
    gguf_close(ctx);

    /* store vocab and build reverse lookup */
    vocab    = tok_st.strings;
    vocab_sz = tok_st.count;
    {
        uint32_t i;
        for (i = 0; i < vocab_sz; i++)
            rv_insert(vocab[i], i);
    }

    /* build merge hash table from "left right" strings */
    if (merge_st.strings) {
        uint32_t i;
        for (i = 0; i < merge_st.count; i++) {
            const char *ms = merge_st.strings[i];
            const char *spc = strchr(ms, ' ');
            if (spc) {
                size_t ll = (size_t)(spc - ms);
                char lstr[256], rstr[256];
                if (ll < 256 && strlen(spc + 1) < 256) {
                    memcpy(lstr, ms, ll); lstr[ll] = '\0';
                    memcpy(rstr, spc + 1, strlen(spc + 1) + 1);
                    uint32_t lid, rid;
                    if (rv_lookup(lstr, &lid) && rv_lookup(rstr, &rid))
                        mr_insert(lid, rid, i);
                }
            }
            free(merge_st.strings[i]);
        }
        free(merge_st.strings);
    }

    fprintf(stderr, "load-gguf-vocab: %u tokens loaded from '%s'\n", vocab_sz, path);
    return (L)vocab_sz;
}

/* ============================================================
 * (token->str id)
 *
 * Returns the decoded string for a single token ID.
 * e.g. (token->str 5868) => " a"
 * ============================================================ */

static L f_token_to_str(L t, L e)
{
    L arg = car(evlis(t, e));
    uint32_t id = (uint32_t)arg;
    if (!vocab || id >= vocab_sz) return l_err;
    init_b2u();
    char out[512];
    tok_decode(vocab[id], out);
    return atom(out);
}

/* ============================================================
 * (detokenize tensor)
 *
 * Concatenates the decoded strings for all token IDs in a
 * rank-1 tensor and returns the result as an atom.
 * ============================================================ */

static L f_detokenize(L t, L e)
{
    L arg = car(evlis(t, e));
    if (T(arg) != TENS || !vocab) return l_err;
    init_b2u();

    tensor_t *ta = tensor_heap + ord(arg);
    /* upper bound: each token can decode to at most ~8 bytes */
    char *buf = (char *)malloc((size_t)ta->len * 8 + 1);
    if (!buf) return l_err;
    int pos = 0;
    II i;
    for (i = 0; i < ta->len; i++) {
        uint32_t id = (uint32_t)ta->data[i];
        if (id >= vocab_sz) continue;
        char tmp[64];
        int n = tok_decode(vocab[id], tmp);
        memcpy(buf + pos, tmp, (size_t)n);
        pos += n;
    }
    buf[pos] = '\0';
    L result = atom(buf);
    free(buf);
    return result;
}

/* ============================================================
 * (tokenize "text")
 *
 * BPE-encodes the input string and returns a rank-1 tensor of
 * token IDs.  Requires load-gguf-vocab to have been called.
 *
 * Algorithm:
 *   1. Map each input byte to its GPT-2 unicode codepoint.
 *   2. Look up each single-char codepoint as a base token ID.
 *   3. Repeatedly merge the adjacent pair with the lowest
 *      merge rank until no more merges apply.
 *   4. Return the resulting IDs as a float tensor.
 * ============================================================ */

typedef struct bpe_node {
    uint32_t         id;
    struct bpe_node *prev, *next;
} bpe_node_t;

static L f_tokenize(L t, L e)
{
    L arg = car(evlis(t, e));
    if (T(arg) != STR && T(arg) != ATOM) return l_err;
    if (!vocab || !rvocab || !merge_ht) {
        fprintf(stderr, "tokenize: call (load-gguf-vocab) first\n");
        return l_err;
    }
    init_b2u();

    const char *text = A + ord(arg);
    size_t tlen = strlen(text);
    if (tlen == 0) {
        II shape[1] = {0};
        tensor_t *bt = alloc_tensor(1, shape, 0, NULL);
        return box(TENS, (II)(bt - tensor_heap));
    }

    /* Step 1+2: one node per input byte, id = base token for that byte */
    bpe_node_t *nodes = (bpe_node_t *)malloc(tlen * sizeof(bpe_node_t));
    if (!nodes) return l_err;

    int n = 0;
    size_t i;
    for (i = 0; i < tlen; i++) {
        uint32_t cp = b2u[(uint8_t)text[i]];
        char tok_str[5];
        tok_str[utf8_encode(cp, tok_str)] = '\0';
        uint32_t id;
        if (!rv_lookup(tok_str, &id)) continue;   /* skip unmapped bytes */
        nodes[n].id   = id;
        nodes[n].prev = n > 0 ? &nodes[n - 1] : NULL;
        nodes[n].next = NULL;
        if (n > 0) nodes[n - 1].next = &nodes[n];
        n++;
    }

    /* Step 3: BPE merges — find lowest-rank pair, merge, repeat */
    int changed = 1;
    while (changed && n > 1) {
        changed = 0;
        uint32_t best_rank = (uint32_t)-1;
        bpe_node_t *best = NULL;
        bpe_node_t *nd;
        for (nd = nodes; nd && nd->next; nd = nd->next) {
            uint32_t rank;
            if (mr_lookup(nd->id, nd->next->id, &rank) && rank < best_rank) {
                best_rank = rank;
                best = nd;
            }
        }
        if (!best) break;

        /* concatenate left+right token strings to find merged token ID */
        const char *ls = vocab[best->id];
        const char *rs = vocab[best->next->id];
        size_t ll = strlen(ls), rl = strlen(rs);
        if (ll + rl < 511) {
            char merged[512];
            memcpy(merged, ls, ll);
            memcpy(merged + ll, rs, rl);
            merged[ll + rl] = '\0';
            uint32_t merged_id;
            if (rv_lookup(merged, &merged_id)) {
                bpe_node_t *rem = best->next;
                best->id   = merged_id;
                best->next = rem->next;
                if (rem->next) rem->next->prev = best;
                n--;
                changed = 1;
            }
        }
    }

    /* Step 4: collect IDs into a float array for the tensor */
    float *ids = (float *)malloc((size_t)n * sizeof(float));
    if (!ids) { free(nodes); return l_err; }
    int idx = 0;
    bpe_node_t *nd;
    for (nd = nodes; nd; nd = nd->next)
        ids[idx++] = (float)nd->id;
    free(nodes);

    II shape[1] = {(II)idx};
    tensor_t *bt = alloc_tensor(1, shape, (II)idx, ids);
    free(ids);
    return box(TENS, (II)(bt - tensor_heap));
}

/* ============================================================
 * Registration
 * ============================================================ */

void register_tokenizer_prims(void)
{
    register_prim("load-gguf-vocab", f_load_gguf_vocab);
    register_prim("token->str",      f_token_to_str);
    register_prim("detokenize",      f_detokenize);
    register_prim("tokenize",        f_tokenize);
}

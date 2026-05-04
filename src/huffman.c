#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "huffman.h"

/* =================================================================
 * Huffman fixed header size:
 *   sizeof(size_t) bytes  = original data length  (8 bytes on x64)
 *   256 bytes             = one code-length byte per symbol
 *   Total                 = 264 bytes
 *
 * For any input shorter than ~264 bytes, the header alone makes the
 * output larger than the input.  We add a 1-byte flag at the very
 * front of every Huffman-encoded block:
 *
 *   flag = 0x00  ->  raw store  (data follows the flag unchanged)
 *   flag = 0x01  ->  Huffman encoded  (header + bitstream follows)
 *
 * Break-even: at 500 KB blocks (the SRS default), the 264-byte
 * header is 0.05% of the block — completely negligible.
 * ================================================================= */
#define HUFFMAN_HEADER_SIZE  (sizeof(size_t) + 256)
#define FLAG_RAW      0x00
#define FLAG_HUFFMAN  0x01

/* ========================= */
/* ===== MIN HEAP ========== */
/* ========================= */

typedef struct {
    HuffmanNode **data;
    int size;
    int capacity;
} MinHeap;

static MinHeap *create_heap(int capacity) {
    MinHeap *h = malloc(sizeof(MinHeap));
    h->data     = malloc(sizeof(HuffmanNode *) * capacity);
    h->size     = 0;
    h->capacity = capacity;
    return h;
}

static void swap_nodes(HuffmanNode **a, HuffmanNode **b) {
    HuffmanNode *t = *a; *a = *b; *b = t;
}

static void heapify_up(MinHeap *h, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (h->data[parent]->freq <= h->data[idx]->freq) break;
        swap_nodes(&h->data[parent], &h->data[idx]);
        idx = parent;
    }
}

static void heapify_down(MinHeap *h, int idx) {
    while (1) {
        int smallest = idx;
        int left  = 2 * idx + 1;
        int right  = 2 * idx + 2;
        if (left  < h->size && h->data[left]->freq  < h->data[smallest]->freq) smallest = left;
        if (right < h->size && h->data[right]->freq < h->data[smallest]->freq) smallest = right;
        if (smallest == idx) break;
        swap_nodes(&h->data[idx], &h->data[smallest]);
        idx = smallest;
    }
}

static void heap_push(MinHeap *h, HuffmanNode *node) {
    h->data[h->size] = node;
    heapify_up(h, h->size);
    h->size++;
}

static HuffmanNode *heap_pop(MinHeap *h) {
    HuffmanNode *root = h->data[0];
    h->data[0] = h->data[--h->size];
    heapify_down(h, 0);
    return root;
}

/* ========================= */
/* ===== TREE OPS ========== */
/* ========================= */

static HuffmanNode *make_node(unsigned char sym, int freq) {
    HuffmanNode *n = malloc(sizeof(HuffmanNode));
    n->symbol = sym;
    n->freq   = freq;
    n->left   = n->right = NULL;
    return n;
}

static void free_tree(HuffmanNode *n) {
    if (!n) return;
    free_tree(n->left);
    free_tree(n->right);
    free(n);
}

void build_huffman_tree(int *freq, HuffmanNode **root) {
    /* Capacity = 2*256 - 1 = 511 (all leaf + internal nodes) */
    MinHeap *h = create_heap(512);

    int distinct = 0;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            heap_push(h, make_node((unsigned char)i, freq[i]));
            distinct++;
        }
    }

    /* Edge case: only one distinct symbol — give it length 1 */
    if (distinct == 1) {
        HuffmanNode *only  = heap_pop(h);
        HuffmanNode *dummy = make_node(0, 0);
        HuffmanNode *par   = make_node(0, only->freq);
        par->left  = only;
        par->right = dummy;
        heap_push(h, par);
    }

    while (h->size > 1) {
        HuffmanNode *left  = heap_pop(h);
        HuffmanNode *right = heap_pop(h);
        HuffmanNode *par   = make_node(0, left->freq + right->freq);
        par->left  = left;
        par->right = right;
        heap_push(h, par);
    }

    *root = (h->size == 1) ? heap_pop(h) : NULL;
    free(h->data);
    free(h);
}

/* ========================= */
/* ===== CODE LENGTHS ====== */
/* ========================= */

static void get_lengths(HuffmanNode *node, int depth, int *lengths) {
    if (!node) return;
    if (!node->left && !node->right) {
        lengths[node->symbol] = depth;
        return;
    }
    get_lengths(node->left,  depth + 1, lengths);
    get_lengths(node->right, depth + 1, lengths);
}

/* ========================= */
/* ===== CANONICAL CODES === */
/* ========================= */

typedef struct { unsigned char symbol; int length; } SymLen;

static int cmp_symlen(const void *a, const void *b) {
    const SymLen *x = (const SymLen *)a;
    const SymLen *y = (const SymLen *)b;
    if (x->length != y->length) return x->length - y->length;
    return (int)x->symbol - (int)y->symbol;
}

static void generate_codes(int *lengths, HuffmanCode *codes) {
    /* Always zero-init so unused symbols have length=0, code=0 */
    for (int i = 0; i < 256; i++) {
        codes[i].code   = 0;
        codes[i].length = 0;
    }

    SymLen arr[256];
    int count = 0;
    for (int i = 0; i < 256; i++) {
        if (lengths[i] > 0) {
            arr[count].symbol = (unsigned char)i;
            arr[count].length = lengths[i];
            count++;
        }
    }

    qsort(arr, count, sizeof(SymLen), cmp_symlen);

    int code = 0, prev_len = 0;
    for (int i = 0; i < count; i++) {
        code <<= (arr[i].length - prev_len);
        codes[arr[i].symbol].code   = (unsigned short)code;
        codes[arr[i].symbol].length = (unsigned char)arr[i].length;
        prev_len = arr[i].length;
        code++;
    }
}

/* Public wrapper used by main if needed */
void generate_canonical_codes(HuffmanNode *root, HuffmanCode *codes) {
    int lengths[256] = {0};
    get_lengths(root, 0, lengths);
    generate_codes(lengths, codes);
}

/* ========================= */
/* ===== ENCODE ============ */
/* ========================= */

void huffman_encode(unsigned char *input, size_t len,
                    unsigned char *output, size_t *out_len)
{
    /*
     * If the input is too small to benefit from Huffman coding,
     * store it raw.  The flag byte tells the decoder which path to take.
     */
    if (len <= HUFFMAN_HEADER_SIZE) {
        output[0] = FLAG_RAW;
        memcpy(output + 1, input, len);
        *out_len = 1 + len;
        return;
    }

    /* --- Normal Huffman path --- */
    output[0] = FLAG_HUFFMAN;
    size_t j  = 1;

    /* Build frequency table */
    int freq[256] = {0};
    for (size_t i = 0; i < len; i++) freq[input[i]]++;

    /* Build tree */
    HuffmanNode *root = NULL;
    build_huffman_tree(freq, &root);

    /* Extract code lengths */
    int lengths[256] = {0};
    get_lengths(root, 0, lengths);
    free_tree(root);

    /* Generate canonical codes */
    HuffmanCode codes[256];
    generate_codes(lengths, codes);

    /* Write original length */
    memcpy(output + j, &len, sizeof(size_t));
    j += sizeof(size_t);

    /* Write code lengths (256 bytes) */
    for (int i = 0; i < 256; i++)
        output[j++] = (unsigned char)lengths[i];

    /* Write encoded bitstream */
    unsigned int  buf  = 0;
    int           bits = 0;

    for (size_t i = 0; i < len; i++) {
        HuffmanCode hc = codes[input[i]];
        buf   = (buf << hc.length) | hc.code;
        bits += hc.length;
        while (bits >= 8) {
            bits -= 8;
            output[j++] = (unsigned char)((buf >> bits) & 0xFF);
        }
    }
    if (bits > 0)
        output[j++] = (unsigned char)((buf << (8 - bits)) & 0xFF);

    *out_len = j;
}

/* ========================= */
/* ===== DECODE ============ */
/* ========================= */

void huffman_decode(unsigned char *input, size_t len,
                    unsigned char *output, size_t *out_len)
{
    if (len == 0) { *out_len = 0; return; }

    /* Read the flag byte */
    unsigned char flag = input[0];

    if (flag == FLAG_RAW) {
        /* Raw store: everything after the flag is the original data */
        size_t data_len = len - 1;
        memcpy(output, input + 1, data_len);
        *out_len = data_len;
        return;
    }

    /* --- Normal Huffman decode path --- */
    size_t i = 1;

    /* Read original length */
    size_t expected_len;
    memcpy(&expected_len, input + i, sizeof(size_t));
    i += sizeof(size_t);

    /* Read code lengths */
    int lengths[256];
    for (int k = 0; k < 256; k++)
        lengths[k] = input[i++];

    /* Reconstruct canonical codes */
    HuffmanCode codes[256];
    generate_codes(lengths, codes);

    /* Find max code length (to know how many bits to refill) */
    int max_len = 0;
    for (int s = 0; s < 256; s++)
        if (codes[s].length > max_len) max_len = codes[s].length;

    /* Decode bitstream */
    unsigned int buf  = 0;
    int          bits = 0;
    size_t       j    = 0;

    while (j < expected_len) {
        /* Refill buffer until we have at least max_len bits */
        while (bits < max_len && i < len) {
            buf   = (buf << 8) | input[i++];
            bits += 8;
        }
        if (bits == 0) break;   /* exhausted input */

        /* Find matching code */
        int found = 0;
        for (int s = 0; s < 256; s++) {
            if (codes[s].length == 0 || bits < codes[s].length) continue;
            unsigned int val = (buf >> (bits - codes[s].length)) &
                               ((1u << codes[s].length) - 1);
            if (val == (unsigned int)codes[s].code) {
                output[j++] = (unsigned char)s;
                bits -= codes[s].length;
                found = 1;
                break;
            }
        }
        if (!found) break;      /* padding bits at end of stream */
    }

    *out_len = j;
}

/* ========================= */
/* ===== HEADER HELPERS ==== */
/* ========================= */

void write_header(HuffmanCode *codes, unsigned char *output, size_t *out_len) {
    size_t j = 0;
    for (int i = 0; i < 256; i++)
        output[j++] = codes[i].length;
    *out_len = j;
}

void encode_data(unsigned char *input, size_t len,
                 HuffmanCode *codes, unsigned char *output, size_t *out_len) {
    size_t j = 0;
    unsigned int buf  = 0;
    int          bits = 0;

    for (size_t i = 0; i < len; i++) {
        HuffmanCode hc = codes[input[i]];
        buf   = (buf << hc.length) | hc.code;
        bits += hc.length;
        while (bits >= 8) {
            bits -= 8;
            output[j++] = (unsigned char)((buf >> bits) & 0xFF);
        }
    }
    if (bits > 0)
        output[j++] = (unsigned char)((buf << (8 - bits)) & 0xFF);
    *out_len = j;
}
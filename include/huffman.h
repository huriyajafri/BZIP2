#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <stddef.h>

typedef struct {
    unsigned short code;
    unsigned char length;
} HuffmanCode;

typedef struct Node {
    unsigned char symbol;
    int freq;
    struct Node *left, *right;
} HuffmanNode;

void build_huffman_tree(int *freq, HuffmanNode **root);
void generate_canonical_codes(HuffmanNode *root, HuffmanCode *codes);
void huffman_encode(unsigned char *input, size_t len,
                    unsigned char *output, size_t *out_len);
void huffman_decode(unsigned char *input, size_t len,
                    unsigned char *output, size_t *out_len);

#endif
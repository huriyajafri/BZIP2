#include <string.h>
#include <stdlib.h>
#include "bwt.h"

static const unsigned char *g_bwt_input = NULL;
static size_t g_bwt_len = 0;

static int compare_rotation_indices(const void *a, const void *b) {
    size_t ia = *(const size_t *)a;
    size_t ib = *(const size_t *)b;
    for (size_t k = 0; k < g_bwt_len; k++) {
        unsigned char ca = g_bwt_input[(ia + k) % g_bwt_len];
        unsigned char cb = g_bwt_input[(ib + k) % g_bwt_len];
        if (ca < cb) return -1;
        if (ca > cb) return 1;
    }
    return 0;
}

int compare_rotations(const void *a, const void *b) {
    Rotation *r1 = (Rotation *)a;
    Rotation *r2 = (Rotation *)b;
    return strcmp(r1->rotation, r2->rotation);
}

void bwt_encode(unsigned char *input, size_t len,
                unsigned char *output, int *primary_index) {
    if (len == 0) {
        *primary_index = 0;
        return;
    }

    size_t *indices = malloc(len * sizeof(size_t));
    if (!indices) {
        *primary_index = 0;
        return;
    }

    for (size_t i = 0; i < len; i++) {
        indices[i] = i;
    }

    g_bwt_input = input;
    g_bwt_len = len;
    qsort(indices, len, sizeof(size_t), compare_rotation_indices);

    for (size_t i = 0; i < len; i++) {
        size_t start = indices[i];
        output[i] = input[(start + len - 1) % len];

        if (start == 0) {
            *primary_index = (int)i;
        }
    }

    free(indices);
}

void bwt_decode(unsigned char *input, size_t len,
                int primary_index, unsigned char *output) {
    if (len == 0) {
        return;
    }
    if (primary_index < 0 || (size_t)primary_index >= len) {
        return;
    }

    int counts[256] = {0};
    for (size_t i = 0; i < len; i++) {
        counts[input[i]]++;
    }

    int starts[256] = {0};
    int total = 0;
    for (int c = 0; c < 256; c++) {
        starts[c] = total;
        total += counts[c];
    }

    int occ[256] = {0};
    int *next = malloc(len * sizeof(int));
    if (!next) {
        return;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = input[i];
        int first_col_index = starts[c] + occ[c];
        next[first_col_index] = (int)i;
        occ[c]++;
    }

    int row = primary_index;
    for (size_t out = 0; out < len; out++) {
        row = next[row];
        output[out] = input[row];
    }

    free(next);
}

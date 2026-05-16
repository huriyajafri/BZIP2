#include <string.h>
#include <stdlib.h>
#include "bwt.h"

static const unsigned char *g_bwt_input = NULL;
static size_t g_bwt_len = 0;

static int cmp_indices(size_t ia, size_t ib) {
    for (size_t k = 0; k < g_bwt_len; k++) {
        unsigned char ca = g_bwt_input[(ia + k) % g_bwt_len];
        unsigned char cb = g_bwt_input[(ib + k) % g_bwt_len];
        if (ca < cb) return -1;
        if (ca > cb) return 1;
    }
    return 0;
}

static void merge_indices(size_t *arr, size_t *tmp, size_t left, size_t mid, size_t right) {
    size_t i = left, j = mid, k = left;
    while (i < mid && j < right) {
        if (cmp_indices(arr[i], arr[j]) <= 0) {
            tmp[k++] = arr[i++];
        } else {
            tmp[k++] = arr[j++];
        }
    }
    while (i < mid) tmp[k++] = arr[i++];
    while (j < right) tmp[k++] = arr[j++];
    for (i = left; i < right; i++) arr[i] = tmp[i];
}

static void merge_sort_indices(size_t *arr, size_t *tmp, size_t n) {
    for (size_t width = 1; width < n; width *= 2) {
        for (size_t left = 0; left < n; left += 2 * width) {
            size_t mid = left + width;
            size_t right = left + 2 * width;
            if (mid > n) mid = n;
            if (right > n) right = n;
            if (mid < right) merge_indices(arr, tmp, left, mid, right);
        }
    }
}

static int compare_rotation_indices(const void *a, const void *b) {
    return cmp_indices(*(const size_t *)a, *(const size_t *)b);
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
    size_t *tmp = malloc(len * sizeof(size_t));
    if (!indices || !tmp) {
        free(indices);
        free(tmp);
        *primary_index = 0;
        return;
    }

    for (size_t i = 0; i < len; i++) {
        indices[i] = i;
    }

    g_bwt_input = input;
    g_bwt_len = len;
    merge_sort_indices(indices, tmp, len);

    for (size_t i = 0; i < len; i++) {
        size_t start = indices[i];
        output[i] = input[(start + len - 1) % len];

        if (start == 0) {
            *primary_index = (int)i;
        }
    }

    free(indices);
    free(tmp);
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
    int *lf = malloc(len * sizeof(int));
    if (!lf) {
        return;
    }

    /* LF(i) = C[L[i]] + Occ(L[i], i) — walk backward from primary index */
    for (size_t i = 0; i < len; i++) {
        unsigned char c = input[i];
        lf[i] = starts[c] + occ[c];
        occ[c]++;
    }

    int row = primary_index;
    for (size_t out = len; out > 0; out--) {
        output[out - 1] = input[row];
        row = lf[row];
    }

    free(lf);
}

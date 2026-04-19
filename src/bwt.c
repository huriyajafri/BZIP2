#include <string.h>
#include <stdlib.h>
#include "bwt.h"

int compare_rotations(const void *a, const void *b) {
    Rotation *r1 = (Rotation *)a;
    Rotation *r2 = (Rotation *)b;
    return strcmp(r1->rotation, r2->rotation);
}
void bwt_encode(unsigned char *input, size_t len,
                unsigned char *output, int *primary_index) {

    Rotation *rotations = malloc(len * sizeof(Rotation));

    // create rotations
    for (int i = 0; i < len; i++) {
        rotations[i].rotation = malloc(len + 1);

        for (int j = 0; j < len; j++) {
            rotations[i].rotation[j] = input[(i + j) % len];
        }

        rotations[i].rotation[len] = '\0';
        rotations[i].index = i;
    }

    // sort rotations
    qsort(rotations, len, sizeof(Rotation), compare_rotations);

    // build output
    for (int i = 0; i < len; i++) {
        output[i] = rotations[i].rotation[len - 1];

        // find original string index
        if (rotations[i].index == 0) {
            *primary_index = i;
        }
    }

    // free memory
    for (int i = 0; i < len; i++) {
        free(rotations[i].rotation);
    }
    free(rotations);
}
void bwt_decode(unsigned char *input, size_t len,
                int primary_index, unsigned char *output) {

    char table[len][len];

    // initialize empty table
    for (int i = 0; i < len; i++)
        for (int j = 0; j < len; j++)
            table[i][j] = '\0';

    // reconstruct table
    for (int i = 0; i < len; i++) {

        // shift right
        for (int j = 0; j < len; j++) {
            memmove(&table[j][1], &table[j][0], len - 1);
            table[j][0] = input[j];
        }

        // sort rows
        qsort(table, len, sizeof(table[0]), (int (*)(const void*, const void*))strcmp);
    }

    // original string
    for (int i = 0; i < len; i++) {
        output[i] = table[primary_index][i];
    }
}

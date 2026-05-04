#include <stdio.h>
#include <stdlib.h>
#include "mtf.h"

/* ========================= */
/* ===== MTF ENCODE ======== */
/* ========================= */

void mtf_encode(unsigned char *input, size_t len, unsigned char *output) {

    unsigned char symbols[256];

    /* Initialize symbol table */
    for (int i = 0; i < 256; i++)
        symbols[i] = i;

    for (size_t i = 0; i < len; i++) {

        unsigned char c = input[i];
        int index = 0;

        /* Find index */
        while (symbols[index] != c)
            index++;

        output[i] = index;

        /* Move to front */
        for (int j = index; j > 0; j--)
            symbols[j] = symbols[j - 1];

        symbols[0] = c;
    }
}

/* ========================= */
/* ===== MTF DECODE ======== */
/* ========================= */

void mtf_decode(unsigned char *input, size_t len, unsigned char *output) {

    unsigned char symbols[256];

    /* Initialize symbol table */
    for (int i = 0; i < 256; i++)
        symbols[i] = i;

    for (size_t i = 0; i < len; i++) {

        int index = input[i];
        unsigned char c = symbols[index];

        output[i] = c;

        /* Move to front */
        for (int j = index; j > 0; j--)
            symbols[j] = symbols[j - 1];

        symbols[0] = c;
    }
}
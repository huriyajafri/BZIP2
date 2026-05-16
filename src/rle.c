#include <string.h>
#include "rle.h"

/* =================================================================
 * RLE-1  —  run-length encoding before BWT (unambiguous binary format)
 * =================================================================
 *
 * Only encodes runs of >= 4 identical bytes using an escape marker so
 * BWT output cannot trigger false expansions during decode.
 *
 *   0xFE CH EXTRA       -> (4 + EXTRA) copies of CH, EXTRA in 0..255
 *   0xFE 0xFE           -> literal byte 0xFE
 *   0xFF                -> literal byte 0xFF
 *   any other byte      -> literal byte
 * ================================================================= */
#define RLE1_THRESHOLD 4
#define RLE1_RUN 0xFE
#define RLE1_LIT_FF 0xFF

void rle1_encode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len)
{
    size_t i = 0, j = 0;

    while (i < len) {
        unsigned char ch = input[i];
        size_t run = 1;

        while (i + run < len &&
               input[i + run] == ch &&
               run < (size_t)(RLE1_THRESHOLD + 255))
            run++;

        if (run >= RLE1_THRESHOLD) {
            output[j++] = RLE1_RUN;
            output[j++] = ch;
            output[j++] = (unsigned char)(run - RLE1_THRESHOLD);
            i += run;
        } else if (ch == RLE1_RUN) {
            output[j++] = RLE1_RUN;
            output[j++] = RLE1_RUN;
            i++;
        } else if (ch == RLE1_LIT_FF) {
            output[j++] = RLE1_LIT_FF;
            i++;
        } else {
            output[j++] = input[i++];
        }
    }

    *out_len = j;
}

void rle1_decode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len)
{
    size_t i = 0, j = 0;

    while (i < len) {
        if (input[i] == RLE1_RUN) {
            if (i + 1 >= len) {
                break;
            }
            if (input[i + 1] == RLE1_RUN) {
                output[j++] = RLE1_RUN;
                i += 2;
            } else if (i + 2 < len) {
                unsigned char ch = input[i + 1];
                unsigned char extra = input[i + 2];
                unsigned int total = (unsigned int)extra + RLE1_THRESHOLD;
                i += 3;
                for (unsigned int k = 0; k < total; k++) {
                    output[j++] = ch;
                }
            } else {
                break;
            }
        } else if (input[i] == RLE1_LIT_FF) {
            output[j++] = RLE1_LIT_FF;
            i++;
        } else {
            output[j++] = input[i++];
        }
    }

    *out_len = j;
}

/* =================================================================
 * RLE-2  —  zero-run encoding applied after MTF, before Huffman
 * =================================================================
 *
 * MTF output is dominated by zeros (index 0). Unambiguous encoding:
 *   0x00              -> literal MTF index 0 (one zero byte)
 *   0xFF 0xFF         -> literal MTF index 255
 *   0xFF (N-2)        -> run of N zeros, N >= 2 (count byte 0..254)
 *   any other byte    -> literal MTF index (1..254)
 * ================================================================= */
#define RLE2_ESC 0xFF

void rle2_encode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len)
{
    /* Pass-through: lossless for all MTF outputs until zero-run scheme is finalized */
    memcpy(output, input, len);
    *out_len = len;
}

void rle2_decode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len)
{
    memcpy(output, input, len);
    *out_len = len;
}
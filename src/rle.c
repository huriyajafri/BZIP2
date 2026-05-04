#include <string.h>
#include "rle.h"

/* =================================================================
 * RLE-1  —  bzip2-style pre/post BWT encoding
 * =================================================================
 *
 * Only encodes runs of >= 4 identical bytes.
 * Shorter runs are copied literally, so inputs without long runs
 * pass through COMPLETELY UNCHANGED (no size increase).
 *
 * Encoded run format:   B B B B COUNT
 *   B     = the repeated byte (4 literal copies)
 *   COUNT = extra repetitions beyond the first 4  (value 0..255)
 *   Total bytes represented = 4 + COUNT
 *
 * Examples:
 *   "banana$"  (7 bytes)  ->  "banana$"       (7 bytes, no runs >= 4)
 *   "aaaa"     (4 bytes)  ->  'a''a''a''a' 0  (5 bytes, COUNT=0)
 *   "aaaaaa"   (6 bytes)  ->  'a''a''a''a' 2  (5 bytes, COUNT=2)
 *   "aaa"      (3 bytes)  ->  "aaa"            (3 bytes, literal)
 * ================================================================= */
#define RLE1_THRESHOLD 4

void rle1_encode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len)
{
    size_t i = 0, j = 0;

    while (i < len) {
        unsigned char ch = input[i];
        size_t run = 1;

        /* Count identical bytes, hard cap at 4+255=259 per token */
        while (i + run < len &&
               input[i + run] == ch &&
               run < (size_t)(RLE1_THRESHOLD + 255))
            run++;

        if (run >= RLE1_THRESHOLD) {
            output[j++] = ch;
            output[j++] = ch;
            output[j++] = ch;
            output[j++] = ch;
            output[j++] = (unsigned char)(run - RLE1_THRESHOLD);
            i += run;
        } else {
            /* Copy every byte of this short run literally */
            for (size_t k = 0; k < run; k++)
                output[j++] = input[i + k];
            i += run;
        }
    }

    *out_len = j;
}

void rle1_decode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len)
{
    size_t i = 0, j = 0;

    while (i < len) {
        unsigned char ch = input[i++];
        output[j++] = ch;

        /*
         * Trigger: the last 4 output bytes are all identical.
         * The NEXT input byte is then a COUNT, not more literal data.
         */
        if (j >= (size_t)RLE1_THRESHOLD &&
            output[j-1] == output[j-2] &&
            output[j-2] == output[j-3] &&
            output[j-3] == output[j-4])
        {
            if (i >= len) break;               /* malformed: missing count */
            unsigned char extra = input[i++];
            for (unsigned int k = 0; k < (unsigned int)extra; k++)
                output[j++] = ch;
        }
    }

    *out_len = j;
}

/* =================================================================
 * RLE-2  —  zero-run encoding applied after MTF, before Huffman
 * =================================================================
 *
 * MTF output is dominated by zeros (recently seen symbols score 0).
 * We compress runs of zeros using an escape scheme:
 *
 *   Non-zero byte          ->  copied as-is         (1 byte -> 1 byte)
 *   Run of N zeros (N>=1)  ->  0x00  (N-1)          (N bytes -> 2 bytes)
 *
 * The count byte stores (N-1), i.e. it is 0-based:
 *   count=0  means 1 zero   (break even: 1 -> 2 bytes)
 *   count=1  means 2 zeros  (break even: 2 -> 2 bytes)
 *   count=2  means 3 zeros  (saves 1 byte)
 *   count=N-1 means N zeros (saves N-2 bytes)
 *
 * This scheme is ALWAYS unambiguous because:
 *   - Every 0x00 in the encoded stream is ALWAYS followed by its count byte.
 *   - Non-zero bytes are never 0x00, so they are never misread as escapes.
 *   - The decoder consumes exactly 2 bytes per zero-run, regardless of count.
 *
 * Single zeros do expand 1->2 bytes. This is the fundamental cost of
 * any byte-level escape scheme and is unavoidable.  At real block sizes
 * (500 KB) the savings on long zero-runs far outweigh this cost.
 * ================================================================= */

void rle2_encode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len)
{
    size_t i = 0, j = 0;

    while (i < len) {
        if (input[i] == 0x00) {
            /* Count the zero run, capped at 256 (count byte max = 255) */
            size_t run = 0;
            while (i < len && input[i] == 0x00 && run < 256) {
                run++;
                i++;
            }
            /* Emit escape + 0-based count */
            output[j++] = 0x00;
            output[j++] = (unsigned char)(run - 1);
        } else {
            output[j++] = input[i++];
        }
    }

    *out_len = j;
}

void rle2_decode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len)
{
    size_t i = 0, j = 0;

    while (i < len) {
        if (input[i] == 0x00) {
            if (i + 1 >= len) {
                /* Malformed: escape with no count — emit one zero and stop */
                output[j++] = 0x00;
                break;
            }
            unsigned char count_minus1 = input[i + 1];
            i += 2;
            unsigned int total = (unsigned int)count_minus1 + 1;
            for (unsigned int k = 0; k < total; k++)
                output[j++] = 0x00;
        } else {
            output[j++] = input[i++];
        }
    }

    *out_len = j;
}
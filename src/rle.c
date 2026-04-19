#include "rle.h"

void rle1_encode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len) {

    size_t i = 0;
    size_t out_index = 0;

    while (i < len) {
        unsigned char current = input[i];
        int count = 1;

        // count repeats
        while (i + 1 < len && input[i] == input[i + 1]) {
            count++;
            i++;
        }

        // store count and character
        output[out_index++] = count;
        output[out_index++] = current;

        i++;
    }

    *out_len = out_index;
}
void rle1_decode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len) {

    size_t i = 0;
    size_t out_index = 0;

    while (i < len) {
        int count = input[i++];
        unsigned char value = input[i++];

        for (int j = 0; j < count; j++) {
            output[out_index++] = value;
        }
    }

    *out_len = out_index;
}

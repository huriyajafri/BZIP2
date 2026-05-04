#ifndef RLE_H
#define RLE_H

#include <stdlib.h>

void rle1_encode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len);

void rle1_decode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len);
                 
void rle2_encode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len);

void rle2_decode(unsigned char *input, size_t len,
                 unsigned char *output, size_t *out_len);                 

#endif

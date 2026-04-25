#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "block.h"
#include "rle.h"
#include "bwt.h"
#include "config.h"

/* Get file size */
long get_file_size(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size;
}

/* Safe print (ASCII + hex fallback) */
void print_bytes(unsigned char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char ch = data[i];

        if (ch >= 32 && ch <= 126) {
            printf("%c", ch);
        }
        // else: skip silently
    }
}

int main() {

    Config config;

    if (load_config("config.ini", &config) != 0) {
        return -1;
    }

    /* FILE SIZE BEFORE */
    long original_size = get_file_size(config.input_file);
    printf("Original file size : %ld bytes\n", original_size);

    /* STEP 1: Divide file into blocks */
    BlockManager *manager =
        divide_into_blocks(config.input_file, config.block_size);

    printf("Blocks: %d\n\n", manager->num_blocks);

    /* ENCODING */
    for (int i = 0; i < manager->num_blocks; i++) {

        Block *block = &manager->blocks[i];

        printf("Encoding block %d (size: %zu bytes)\n", i, block->size);

        unsigned char *rle_output = malloc(block->size * 2 + 10);
        size_t rle_len = 0;

        unsigned char *bwt_output = malloc(block->size * 2 + 10);
        int index = 0;

        /* STEP 2: RLE */
        if (config.rle1_enabled) {
            rle1_encode(block->data, block->size, rle_output, &rle_len);
        } else {
            memcpy(rle_output, block->data, block->size);
            rle_len = block->size;
        }

        printf("  After RLE-1 : %zu bytes\n", rle_len);

        /* STEP 3: BWT */
        if (config.bwt_enabled) {
            bwt_encode(rle_output, rle_len, bwt_output, &index);
        } else {
            memcpy(bwt_output, rle_output, rle_len);
        }

        printf("  After BWT   : %zu bytes (index: %d)\n", rle_len, index);

        printf("  Encoded     : ");
        print_bytes(bwt_output, rle_len);
        printf("\n\n");

        /* Store index + data */
        unsigned char *packed = malloc(4 + rle_len);

        packed[0] = (index >> 0) & 0xFF;
        packed[1] = (index >> 8) & 0xFF;
        packed[2] = (index >> 16) & 0xFF;
        packed[3] = (index >> 24) & 0xFF;

        memcpy(packed + 4, bwt_output, rle_len);

        free(block->data);
        block->data = packed;
        block->size = 4 + rle_len;

        free(rle_output);
        free(bwt_output);
    }

    /* STEP 4: Write compressed file */
    reassemble_blocks(manager, config.output_file);

    /* FILE SIZE AFTER */
    long compressed_size = get_file_size(config.output_file);

    printf("Compressed file size : %ld bytes\n", compressed_size);

    if (original_size > 0) {
        printf("Compression ratio    : %.4f\n\n",
               (double)compressed_size / original_size);
    }

    /* ===================== */
    /* ===== DECODING ====== */
    /* ===================== */

    printf("DECODING\n");

    /* Read original again (for block boundaries) */
    BlockManager *orig =
        divide_into_blocks(config.input_file, config.block_size);

    FILE *fp = fopen(config.output_file, "rb");

    for (int i = 0; i < orig->num_blocks; i++) {

        Block *orig_block = &orig->blocks[i];

        /* Recompute encoded size */
        unsigned char *tmp = malloc(orig_block->size * 2 + 10);
        size_t rle_len = 0;

        if (config.rle1_enabled)
            rle1_encode(orig_block->data, orig_block->size, tmp, &rle_len);
        else
            rle_len = orig_block->size;

        free(tmp);

        size_t comp_size = 4 + rle_len;

        unsigned char *comp = malloc(comp_size);
        fread(comp, 1, comp_size, fp);

        int index =
            comp[0] |
            (comp[1] << 8) |
            (comp[2] << 16) |
            (comp[3] << 24);

        printf("\nDecoding block %d\n", i);

        /* BWT Decode */
        unsigned char *bwt_dec = malloc(rle_len);

        if (config.bwt_enabled)
            bwt_decode(comp + 4, rle_len, index, bwt_dec);
        else
            memcpy(bwt_dec, comp + 4, rle_len);

        /* RLE Decode */
        unsigned char *final = malloc(rle_len * 256);
        size_t final_len = 0;

        if (config.rle1_enabled)
            rle1_decode(bwt_dec, rle_len, final, &final_len);
        else {
            memcpy(final, bwt_dec, rle_len);
            final_len = rle_len;
        }

        printf("  Decoded size : %zu\n", final_len);

        printf("  Output       : ");
        print_bytes(final, final_len);
        printf("\n");

        free(comp);
        free(bwt_dec);
        free(final);
    }

    fclose(fp);
    free_block_manager(manager);
    free_block_manager(orig);

    return 0;
}
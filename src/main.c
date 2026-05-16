#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "block.h"
#include "rle.h"
#include "bwt.h"
#include "config.h"
#include "mtf.h"
#include "huffman.h"

/* Get file size */
long get_file_size(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size;
}

/* Safe print (ASCII only) */
void print_bytes(unsigned char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char ch = data[i];
        if (ch >= 32 && ch <= 126)
            printf("%c", ch);
    }
}

/* helper function */
void debug_print(const char *label, unsigned char *data, size_t len) {
    printf("%s (%zu bytes): ", label, len);

    for (size_t i = 0; i < len && i < 50; i++) {
        if (data[i] >= 32 && data[i] <= 126)
            printf("%c", data[i]);
        else
            printf(".");
    }

    printf("\n");
}

/* ---------------------------------------------------------------
 * save_stage
 * Writes a raw binary snapshot of one pipeline stage to disk.
 *
 * Output path:  stages/<name>.bin
 *   e.g.  stages/block0_rle1.bin
 *         stages/block0_bwt.bin
 *         stages/block0_mtf.bin
 *         stages/block0_rle2.bin
 *         stages/block0_huffman.bin
 *
 * The "stages/" directory is created automatically.
 * If the file cannot be opened a warning is printed and the
 * compression run continues normally — saving is best-effort.
 * --------------------------------------------------------------- */
static void save_stage(const char *name, unsigned char *data, size_t len)
{
    /* Create the output directory (platform-specific, best-effort) */
#ifdef _WIN32
    system("if not exist stages mkdir stages");
#else
    system("mkdir -p stages");
#endif

    char path[256];
    snprintf(path, sizeof(path), "stages/%s.bin", name);

    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "  [stages] Warning: could not write '%s'\n", path);
        return;
    }
    fwrite(data, 1, len, f);
    fclose(f);

    printf("  [stages] Saved %zu bytes  ->  %s\n", len, path);
}

int main() {

    Config config;

    if (load_config("config.ini", &config) != 0) {
        printf("Error loading config\n");
        return -1;
    }

    long original_size = get_file_size(config.input_file);
    printf("Original file size : %ld bytes\n", original_size);

    BlockManager *manager =
        divide_into_blocks(config.input_file, config.block_size);
    if (!manager) {
        printf("Failed to divide file into blocks\n");
        return -1;
    }

    printf("Blocks: %d\n\n", manager->num_blocks);

    /* ===================== */
    /* ===== ENCODING ====== */
    /* ===================== */

    for (int i = 0; i < manager->num_blocks; i++) {

        Block *block = &manager->blocks[i];

        /* FIX: Save original data BEFORE encoding overwrites block->data */
        unsigned char *original_copy = malloc(block->size);
        memcpy(original_copy, block->data, block->size);
        size_t original_len = block->size;

        printf("Encoding block %d (size: %zu bytes)\n", i, block->size);

        /* STEP 1: RLE-1 */
        unsigned char *rle_output = malloc(block->size * 2 + 10);
        size_t rle_len = 0;

        if (config.rle1_enabled)
            rle1_encode(block->data, block->size, rle_output, &rle_len);
        else {
            memcpy(rle_output, block->data, block->size);
            rle_len = block->size;
        }

        if (!config.benchmark_mode) {
            debug_print("  After RLE-1", rle_output, rle_len);
        }

        /* --- Save RLE-1 stage output --- */
        {
            char stage_name[64];
            snprintf(stage_name, sizeof(stage_name), "block%d_rle1", i);
            save_stage(stage_name, rle_output, rle_len);
        }

        /* STEP 2: BWT */
        unsigned char *bwt_output = malloc(rle_len + 10);
        int index = 0;

        if (config.bwt_enabled)
            bwt_encode(rle_output, rle_len, bwt_output, &index);
        else
            memcpy(bwt_output, rle_output, rle_len);

        if (!config.benchmark_mode) {
            printf("BWT Primary Index: %d\n", index);
            debug_print("After BWT", bwt_output, rle_len);
        }

        /* --- Save BWT stage output --- */
        {
            char stage_name[64];
            snprintf(stage_name, sizeof(stage_name), "block%d_bwt", i);
            save_stage(stage_name, bwt_output, rle_len);
        }

        /* STEP 3: MTF */
        unsigned char *mtf_output = malloc(rle_len);
        if (config.mtf_enabled)
            mtf_encode(bwt_output, rle_len, mtf_output);
        else
            memcpy(mtf_output, bwt_output, rle_len);

        if (!config.benchmark_mode) {
            debug_print("After MTF", mtf_output, rle_len);
        }

        /* --- Save MTF stage output --- */
        {
            char stage_name[64];
            snprintf(stage_name, sizeof(stage_name), "block%d_mtf", i);
            save_stage(stage_name, mtf_output, rle_len);
        }

        /* STEP 4: RLE-2 */
        unsigned char *rle2_output = malloc(rle_len * 2 + 10);
        size_t rle2_len = 0;

        if (config.rle2_enabled)
            rle2_encode(mtf_output, rle_len, rle2_output, &rle2_len);
        else {
            memcpy(rle2_output, mtf_output, rle_len);
            rle2_len = rle_len;
        }

        if (!config.benchmark_mode) {
            debug_print("After RLE-2", rle2_output, rle2_len);
        }

        /* --- Save RLE-2 stage output --- */
        {
            char stage_name[64];
            snprintf(stage_name, sizeof(stage_name), "block%d_rle2", i);
            save_stage(stage_name, rle2_output, rle2_len);
        }

        /* STEP 5: Huffman */
        unsigned char *huff_output = malloc(rle2_len * 2 + 256);
        size_t huff_len = 0;

        if (config.huffman_enabled)
            huffman_encode(rle2_output, rle2_len, huff_output, &huff_len);
        else {
            memcpy(huff_output, rle2_output, rle2_len);
            huff_len = rle2_len;
        }

        if (!config.benchmark_mode) {
            debug_print("After Huffman", huff_output, huff_len);
        }

        /* --- Save Huffman stage output --- */
        {
            char stage_name[64];
            snprintf(stage_name, sizeof(stage_name), "block%d_huffman", i);
            save_stage(stage_name, huff_output, huff_len);
        }

        /* PACK: [index (4 bytes)] + [size (4 bytes)] + [data] */
        unsigned char *packed = malloc(8 + huff_len);

        packed[0] = (index >> 0) & 0xFF;
        packed[1] = (index >> 8) & 0xFF;
        packed[2] = (index >> 16) & 0xFF;
        packed[3] = (index >> 24) & 0xFF;

        packed[4] = (huff_len >> 0) & 0xFF;
        packed[5] = (huff_len >> 8) & 0xFF;
        packed[6] = (huff_len >> 16) & 0xFF;
        packed[7] = (huff_len >> 24) & 0xFF;

        memcpy(packed + 8, huff_output, huff_len);

        free(block->data);
        block->data = packed;
        block->size = 8 + huff_len;

        if (!config.benchmark_mode) {
            printf("Final Packed Block Size: %zu bytes\n", block->size);
        }

        free(rle_output);
        free(bwt_output);
        free(mtf_output);
        free(rle2_output);
        free(huff_output);

        /* ===================== */
        /* ===== DECODING ====== */
        /* ===================== */

        if (!config.benchmark_mode) {
            printf("\nDecoding block %d\n", i);
        }

        unsigned char *packed_data = block->data;

        int dec_index =
            packed_data[0] |
            (packed_data[1] << 8) |
            (packed_data[2] << 16) |
            (packed_data[3] << 24);

        unsigned int comp_size =
            (unsigned int)packed_data[4] |
            ((unsigned int)packed_data[5] << 8) |
            ((unsigned int)packed_data[6] << 16) |
            ((unsigned int)packed_data[7] << 24);

        unsigned char *comp = packed_data + 8;

        /* STEP 1: Huffman Decode */
        size_t buf_cap = original_len + comp_size + 256;
        unsigned char *rle2_dec = malloc(buf_cap);
        size_t rle2_dec_len = 0;

        if (config.huffman_enabled)
            huffman_decode(comp, comp_size, rle2_dec, &rle2_dec_len);
        else {
            memcpy(rle2_dec, comp, comp_size);
            rle2_dec_len = comp_size;
        }
        if (!config.benchmark_mode) {
            debug_print("After Huffman Decode", rle2_dec, rle2_dec_len);
        }

        /* STEP 2: RLE-2 Decode */
        unsigned char *mtf_dec = malloc(buf_cap);
        size_t mtf_len = 0;

        if (config.rle2_enabled)
            rle2_decode(rle2_dec, rle2_dec_len, mtf_dec, &mtf_len);
        else {
            memcpy(mtf_dec, rle2_dec, rle2_dec_len);
            mtf_len = rle2_dec_len;
        }
        if (!config.benchmark_mode) {
            debug_print("After RLE-2 Decode", mtf_dec, mtf_len);
        }

        /* STEP 3: MTF Decode */
        unsigned char *bwt_dec = malloc(buf_cap);
        if (config.mtf_enabled)
            mtf_decode(mtf_dec, mtf_len, bwt_dec);
        else
            memcpy(bwt_dec, mtf_dec, mtf_len);
        if (!config.benchmark_mode) {
            debug_print("After MTF Decode", bwt_dec, mtf_len);
        }

        /* STEP 4: BWT Decode */
        size_t bwt_len = mtf_len;
        unsigned char *rle1_dec = malloc(bwt_len);
        if (!rle1_dec) {
            free(rle2_dec);
            free(mtf_dec);
            free(bwt_dec);
            free(original_copy);
            return -1;
        }

        if (config.bwt_enabled)
            bwt_decode(bwt_dec, bwt_len, dec_index, rle1_dec);
        else
            memcpy(rle1_dec, bwt_dec, bwt_len);

        if (!config.benchmark_mode) {
            debug_print("After BWT Decode", rle1_dec, bwt_len);
        }

        /* STEP 5: RLE-1 Decode */
        unsigned char *final = malloc(original_len + 256);
        size_t final_len = 0;
        if (!final) {
            free(rle2_dec);
            free(mtf_dec);
            free(bwt_dec);
            free(rle1_dec);
            free(original_copy);
            return -1;
        }

        if (config.rle1_enabled)
            rle1_decode(rle1_dec, bwt_len, final, &final_len);
        else {
            memcpy(final, rle1_dec, bwt_len);
            final_len = bwt_len;
        }

        if (!config.benchmark_mode) {
            debug_print("Final Output", final, final_len);
        }

        if (!config.benchmark_mode) {
            printf("  Output               : ");
            print_bytes(final, final_len);
            printf("\n");
        }

        /* FIX: compare against original_copy, not the now-overwritten block->data */
        if (final_len == original_len &&
            memcmp(final, original_copy, final_len) == 0) {
            if (!config.benchmark_mode) {
                printf("Block %d VERIFIED SUCCESSFULLY\n", i);
            }
        } else {
            printf("Block %d VERIFICATION FAILED\n", i);
            if (!config.benchmark_mode) {
                free(rle2_dec);
                free(mtf_dec);
                free(bwt_dec);
                free(rle1_dec);
                free(final);
                free(original_copy);
                free_block_manager(manager);
                return -1;
            }
        }

        free(rle2_dec);
        free(mtf_dec);
        free(bwt_dec);
        free(rle1_dec);
        free(final);
        free(original_copy); /* FIX: free the saved copy */
    }

    /* ===================== */
    /* ===== FILE I/O ====== */
    /* ===================== */

    reassemble_blocks(manager, config.output_file);

    long compressed_size = get_file_size(config.output_file);

    if (config.output_metrics) {
        printf("\nCompressed file size : %ld bytes\n", compressed_size);
    }
    if (original_size > 0 && config.output_metrics) {
        printf("Compression ratio    : %.4f\n\n",
               (double)compressed_size / original_size);
    }

    free_block_manager(manager);

    return 0;
}
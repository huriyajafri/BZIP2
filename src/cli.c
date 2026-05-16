#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "block.h"
#include "rle.h"
#include "bwt.h"
#include "config.h"
#include "mtf.h"
#include "huffman.h"

/* -------------------------------------------------- */
/*  Utilities                                         */
/* -------------------------------------------------- */

static long get_file_size(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    return size;
}

static void write_u32(unsigned char *buf, uint32_t v) {
    buf[0] = (v >>  0) & 0xFF;
    buf[1] = (v >>  8) & 0xFF;
    buf[2] = (v >> 16) & 0xFF;
    buf[3] = (v >> 24) & 0xFF;
}

static uint32_t read_u32(const unsigned char *buf) {
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] <<  8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

/* -------------------------------------------------- */
/*  Compress                                          */
/* -------------------------------------------------- */

/*
 * Compressed file layout
 * ----------------------
 *  [4 bytes]  magic "BZ26"
 *  [4 bytes]  num_blocks
 *  per block:
 *    [4 bytes]  BWT primary index
 *    [4 bytes]  original (uncompressed) block size
 *    [4 bytes]  compressed payload size
 *    [N bytes]  compressed payload
 */

static int compress(const char *input_file, const char *output_file,
                    const Config *cfg)
{
    long original_size = get_file_size(input_file);
    if (original_size < 0) {
        fprintf(stderr, "Error: cannot open input file '%s'\n", input_file);
        return -1;
    }

    BlockManager *manager = divide_into_blocks(input_file, cfg->block_size);
    if (!manager) {
        fprintf(stderr, "Error: failed to divide file into blocks\n");
        return -1;
    }

    FILE *out = fopen(output_file, "wb");
    if (!out) {
        fprintf(stderr, "Error: cannot open output file '%s'\n", output_file);
        free_block_manager(manager);
        return -1;
    }

    /* Write magic + block count */
    fwrite("BZ26", 1, 4, out);
    unsigned char hdr[4];
    write_u32(hdr, (uint32_t)manager->num_blocks);
    fwrite(hdr, 1, 4, out);

    printf("Compressing '%s' -> '%s'\n", input_file, output_file);
    printf("Blocks: %d  (block size: %zu bytes)\n\n", manager->num_blocks, cfg->block_size);

    for (int i = 0; i < manager->num_blocks; i++) {
        Block *block = &manager->blocks[i];
        size_t orig_block_len = block->size;

        printf("  Block %d/%d  (%zu bytes)\n", i + 1, manager->num_blocks, orig_block_len);

        /* --- RLE-1 --- */
        unsigned char *rle1_out = malloc(orig_block_len * 2 + 10);
        size_t rle1_len = 0;
        if (cfg->rle1_enabled)
            rle1_encode(block->data, orig_block_len, rle1_out, &rle1_len);
        else { memcpy(rle1_out, block->data, orig_block_len); rle1_len = orig_block_len; }

        /* --- BWT --- */
        unsigned char *bwt_out = malloc(rle1_len + 10);
        int bwt_index = 0;
        if (cfg->bwt_enabled)
            bwt_encode(rle1_out, rle1_len, bwt_out, &bwt_index);
        else { memcpy(bwt_out, rle1_out, rle1_len); }

        /* --- MTF --- */
        unsigned char *mtf_out = malloc(rle1_len + 10);
        if (cfg->mtf_enabled)
            mtf_encode(bwt_out, rle1_len, mtf_out);
        else { memcpy(mtf_out, bwt_out, rle1_len); }

        /* --- RLE-2 --- */
        unsigned char *rle2_out = malloc(rle1_len * 2 + 10);
        size_t rle2_len = 0;
        if (cfg->rle2_enabled)
            rle2_encode(mtf_out, rle1_len, rle2_out, &rle2_len);
        else { memcpy(rle2_out, mtf_out, rle1_len); rle2_len = rle1_len; }

        /* --- Huffman --- */
        unsigned char *huff_out = malloc(rle2_len * 2 + 512);
        size_t huff_len = 0;
        if (cfg->huffman_enabled)
            huffman_encode(rle2_out, rle2_len, huff_out, &huff_len);
        else { memcpy(huff_out, rle2_out, rle2_len); huff_len = rle2_len; }

        /* --- Write block header + payload --- */
        unsigned char blk_hdr[12];
        write_u32(blk_hdr + 0, (uint32_t)bwt_index);
        write_u32(blk_hdr + 4, (uint32_t)orig_block_len);
        write_u32(blk_hdr + 8, (uint32_t)huff_len);
        fwrite(blk_hdr, 1, 12, out);
        fwrite(huff_out, 1, huff_len, out);

        printf("    -> compressed to %zu bytes\n", huff_len);

        free(rle1_out);
        free(bwt_out);
        free(mtf_out);
        free(rle2_out);
        free(huff_out);
    }

    fclose(out);
    free_block_manager(manager);

    long compressed_size = get_file_size(output_file);
    printf("\nOriginal size  : %ld bytes\n", original_size);
    printf("Compressed size: %ld bytes\n", compressed_size);
    printf("Ratio          : %.4f\n", (double)compressed_size / (double)original_size);

    return 0;
}

/* -------------------------------------------------- */
/*  Decompress                                        */
/* -------------------------------------------------- */

static int decompress(const char *input_file, const char *output_file,
                      const Config *cfg)
{
    FILE *in = fopen(input_file, "rb");
    if (!in) {
        fprintf(stderr, "Error: cannot open compressed file '%s'\n", input_file);
        return -1;
    }

    /* Check magic */
    char magic[4];
    if (fread(magic, 1, 4, in) != 4 || memcmp(magic, "BZ26", 4) != 0) {
        fprintf(stderr, "Error: '%s' is not a valid BZ26 compressed file\n", input_file);
        fclose(in);
        return -1;
    }

    unsigned char hdr[4];
    fread(hdr, 1, 4, in);
    int num_blocks = (int)read_u32(hdr);

    printf("Decompressing '%s' -> '%s'\n", input_file, output_file);
    printf("Blocks: %d\n\n", num_blocks);

    FILE *out = fopen(output_file, "wb");
    if (!out) {
        fprintf(stderr, "Error: cannot open output file '%s'\n", output_file);
        fclose(in);
        return -1;
    }

    int ok = 1;

    for (int i = 0; i < num_blocks && ok; i++) {
        /* Read block header */
        unsigned char blk_hdr[12];
        if (fread(blk_hdr, 1, 12, in) != 12) {
            fprintf(stderr, "Error: unexpected end of file reading block %d header\n", i);
            ok = 0; break;
        }
        int      bwt_index     = (int)read_u32(blk_hdr + 0);
        size_t   orig_len      = (size_t)read_u32(blk_hdr + 4);
        size_t   comp_len      = (size_t)read_u32(blk_hdr + 8);

        printf("  Block %d/%d  (orig: %zu bytes, compressed: %zu bytes)\n",
               i + 1, num_blocks, orig_len, comp_len);

        /* Read compressed payload */
        unsigned char *comp = malloc(comp_len);
        if (!comp || fread(comp, 1, comp_len, in) != comp_len) {
            fprintf(stderr, "Error: failed to read block %d payload\n", i);
            free(comp);
            ok = 0; break;
        }

        size_t buf = orig_len * 2 + comp_len + 512;  /* generous decode buffer */

        /* --- Huffman decode --- */
        unsigned char *rle2_dec = malloc(buf);
        size_t rle2_len = 0;
        if (cfg->huffman_enabled)
            huffman_decode(comp, comp_len, rle2_dec, &rle2_len);
        else { memcpy(rle2_dec, comp, comp_len); rle2_len = comp_len; }
        free(comp);

        /* --- RLE-2 decode --- */
        unsigned char *mtf_dec = malloc(buf);
        size_t mtf_len = 0;
        if (cfg->rle2_enabled)
            rle2_decode(rle2_dec, rle2_len, mtf_dec, &mtf_len);
        else { memcpy(mtf_dec, rle2_dec, rle2_len); mtf_len = rle2_len; }
        free(rle2_dec);

        /* --- MTF decode --- */
        unsigned char *bwt_dec = malloc(buf);
        if (cfg->mtf_enabled)
            mtf_decode(mtf_dec, mtf_len, bwt_dec);
        else
            memcpy(bwt_dec, mtf_dec, mtf_len);
        free(mtf_dec);

        /* --- BWT decode --- */
        unsigned char *rle1_dec = malloc(buf);
        size_t bwt_len = mtf_len;
        if (cfg->bwt_enabled)
            bwt_decode(bwt_dec, bwt_len, bwt_index, rle1_dec);
        else
            memcpy(rle1_dec, bwt_dec, bwt_len);
        free(bwt_dec);

        /* --- RLE-1 decode --- */
        unsigned char *final = malloc(buf);
        size_t final_len = 0;
        if (cfg->rle1_enabled)
            rle1_decode(rle1_dec, bwt_len, final, &final_len);
        else { memcpy(final, rle1_dec, bwt_len); final_len = bwt_len; }
        free(rle1_dec);

        /* Sanity check */
        if (final_len != orig_len) {
            fprintf(stderr, "  WARNING: block %d size mismatch (got %zu, expected %zu)\n",
                    i, final_len, orig_len);
        }

        fwrite(final, 1, final_len, out);
        free(final);

        printf("    -> decoded to %zu bytes\n", final_len);
    }

    fclose(in);
    fclose(out);

    if (!ok) {
        remove(output_file);
        return -1;
    }

    printf("\nDone. Output written to '%s'\n", output_file);
    return 0;
}

/* -------------------------------------------------- */
/*  main                                              */
/* -------------------------------------------------- */

static void usage(const char *prog) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <input>  -c <output>   compress\n", prog);
    fprintf(stderr, "  %s <input>  -d <output>   decompress\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        usage(argv[0]);
        return 1;
    }

    const char *input_file  = argv[1];
    const char *mode_flag   = argv[2];
    const char *output_file = argv[3];

    /* Load config for pipeline settings (block size, which stages enabled, etc.) */
    Config cfg;
    if (load_config("config.ini", &cfg) != 0) {
        /* If no config file, use safe defaults */
        memset(&cfg, 0, sizeof(cfg));
        cfg.block_size      = 500000;
        cfg.rle1_enabled    = 1;
        cfg.bwt_enabled     = 1;
        cfg.mtf_enabled     = 1;
        cfg.rle2_enabled    = 1;
        cfg.huffman_enabled = 1;
        cfg.output_metrics  = 1;
    }

    if (strcmp(mode_flag, "-c") == 0) {
        return compress(input_file, output_file, &cfg);
    } else if (strcmp(mode_flag, "-d") == 0) {
        return decompress(input_file, output_file, &cfg);
    } else {
        fprintf(stderr, "Error: unknown flag '%s'\n", mode_flag);
        usage(argv[0]);
        return 1;
    }
}
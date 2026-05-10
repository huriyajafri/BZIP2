#ifndef CONFIG_H
#define CONFIG_H

#include <stdlib.h>

typedef struct {
    size_t block_size;
    int rle1_enabled;
    int bwt_enabled;
    int mtf_enabled;
    int rle2_enabled;
    int huffman_enabled;
    int benchmark_mode;
    int output_metrics;
    char bwt_type[32];
    char input_file[256];
    char output_file[256];
    char input_directory[256];
    char output_directory[256];
} Config;

int load_config(const char *filename, Config *config);

#endif

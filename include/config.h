#ifndef CONFIG_H
#define CONFIG_H

#include <stdlib.h>

typedef struct {
    size_t block_size;
    int rle1_enabled;
    int bwt_enabled;
    char input_file[256];
    char output_file[256];
} Config;

int load_config(const char *filename, Config *config);

#endif

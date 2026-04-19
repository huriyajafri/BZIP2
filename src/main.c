#include <stdio.h>
#include "block.h"
#include "rle.h"
#include "bwt.h"
#include "config.h"

int main() {

    Config config;

    if (load_config("config.ini", &config) != 0) {
        return -1;
    }

    // STEP 1: Divide file into blocks
    BlockManager *manager = divide_into_blocks(config.input_file, config.block_size);

    printf("Blocks: %d\n", manager->num_blocks);

    // Process each block
    for (int i = 0; i < manager->num_blocks; i++) {

        Block *block = &manager->blocks[i];

        unsigned char *rle_output = malloc(1000);
        size_t rle_len;

        unsigned char *bwt_output = malloc(1000);
        int index;

        // STEP 2: RLE
        if (config.rle1_enabled) {
            rle1_encode(block->data, block->size, rle_output, &rle_len);
        }
        printf("RLE Output: ");
        for (int j = 0; j < rle_len; j++) {
            printf("%d ", rle_output[j]);
        }
        printf("\n");

        // STEP 3: BWT
        if (config.bwt_enabled) {
            bwt_encode(rle_output, rle_len, bwt_output, &index);
        }
        printf("BWT Output: ");
        for (int j = 0; j < rle_len; j++) {
            printf("%c", bwt_output[j]);
        }
        printf("\nIndex: %d\n", index);

        // Free old block data and replace with processed data
        free(block->data);
        block->data = bwt_output;
        block->size = rle_len;
        
        free(rle_output);
    }

    // STEP 4: Reassemble
    reassemble_blocks(manager, config.output_file);

    free_block_manager(manager);

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include "block.h"

BlockManager* divide_into_blocks(const char *filename, size_t block_size) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Error opening file\n");
        return NULL;
    }

    BlockManager *manager = malloc(sizeof(BlockManager));
    manager->block_size = block_size;
    manager->num_blocks = 0;
    manager->blocks = NULL;

    while (1) {
        Block block;
        block.data = malloc(block_size);

        size_t bytesRead = fread(block.data, 1, block_size, file);
        if (bytesRead == 0) {
            free(block.data);
            break;
        }

        block.size = bytesRead;
        block.original_size = bytesRead;

        manager->num_blocks++;
        manager->blocks = realloc(manager->blocks, manager->num_blocks * sizeof(Block));
        manager->blocks[manager->num_blocks - 1] = block;
    }

    fclose(file);
    return manager;
}


int reassemble_blocks(BlockManager *manager, const char *output_filename) {
    FILE *file = fopen(output_filename, "wb");
    if (!file) return -1;

    for (int i = 0; i < manager->num_blocks; i++) {
        fwrite(manager->blocks[i].data, 1, manager->blocks[i].size, file);
    }

    fclose(file);
    return 0;
}
void free_block_manager(BlockManager *manager) {
    for (int i = 0; i < manager->num_blocks; i++) {
        free(manager->blocks[i].data);
    }
    free(manager->blocks);
    free(manager);
}

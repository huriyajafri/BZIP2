#include <stdio.h>
#include <string.h>
#include "config.h"

int load_config(const char *filename, Config *config) {

    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error opening config file\n");
        return -1;
    }

    char line[256];

    while (fgets(line, sizeof(line), file)) {

        // remove newline
        line[strcspn(line, "\n")] = 0;

        if (strstr(line, "block_size")) {
            sscanf(line, "block_size = %zu", &config->block_size);
        }

        else if (strstr(line, "rle1_enabled")) {
            char val[10];
            sscanf(line, "rle1_enabled = %s", val);
            config->rle1_enabled = (strcmp(val, "true") == 0);
        }

        else if (strstr(line, "bwt_type")) {
            config->bwt_enabled = 1; // since BWT is required
        }

        else if (strstr(line, "input_file")) {
            sscanf(line, "input_file = %s", config->input_file);
        }

        else if (strstr(line, "output_file")) {
            sscanf(line, "output_file = %s", config->output_file);
        }
    }

    fclose(file);
    return 0;
}

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "config.h"

static void trim(char *s) {
    size_t start = 0;
    size_t len = strlen(s);
    while (start < len && isspace((unsigned char)s[start])) {
        start++;
    }
    if (start > 0) {
        memmove(s, s + start, len - start + 1);
    }
    len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

static int parse_bool(const char *val) {
    return strcmp(val, "true") == 0 || strcmp(val, "1") == 0;
}

static void copy_string(char *dst, size_t dst_size, const char *src) {
    if (dst_size == 0) return;
    snprintf(dst, dst_size, "%s", src);
}

int load_config(const char *filename, Config *config) {
    memset(config, 0, sizeof(*config));
    config->block_size = 500000;
    config->rle1_enabled = 1;
    config->bwt_enabled = 1;
    config->mtf_enabled = 1;
    config->rle2_enabled = 1;
    config->huffman_enabled = 1;
    config->output_metrics = 1;
    strcpy(config->bwt_type, "matrix");
    strcpy(config->input_file, "input.txt");
    strcpy(config->output_file, "output.bz2");
    strcpy(config->input_directory, "./benchmarks/");
    strcpy(config->output_directory, "./results/");

    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error opening config file\n");
        return -1;
    }

    char line[256];

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        char *comment = strchr(line, '#');
        if (comment) *comment = '\0';
        trim(line);
        if (line[0] == '\0' || line[0] == '[') {
            continue;
        }

        char key[64] = {0};
        char value[256] = {0};
        if (sscanf(line, "%63[^=]=%255[^\n]", key, value) != 2) {
            continue;
        }
        trim(key);
        trim(value);

        if (strcmp(key, "block_size") == 0) {
            size_t parsed = 0;
            if (sscanf(value, "%zu", &parsed) == 1) {
                if (parsed < 100000) parsed = 100000;
                if (parsed > 900000) parsed = 900000;
                config->block_size = parsed;
            }
        } else if (strcmp(key, "rle1_enabled") == 0) {
            config->rle1_enabled = parse_bool(value);
        } else if (strcmp(key, "bwt_type") == 0) {
            copy_string(config->bwt_type, sizeof(config->bwt_type), value);
            config->bwt_enabled = 1;
        } else if (strcmp(key, "mtf_enabled") == 0) {
            config->mtf_enabled = parse_bool(value);
        } else if (strcmp(key, "rle2_enabled") == 0) {
            config->rle2_enabled = parse_bool(value);
        } else if (strcmp(key, "huffman_enabled") == 0) {
            config->huffman_enabled = parse_bool(value);
        } else if (strcmp(key, "benchmark_mode") == 0) {
            config->benchmark_mode = parse_bool(value);
        } else if (strcmp(key, "output_metrics") == 0) {
            config->output_metrics = parse_bool(value);
        } else if (strcmp(key, "input_file") == 0) {
            copy_string(config->input_file, sizeof(config->input_file), value);
        } else if (strcmp(key, "output_file") == 0) {
            copy_string(config->output_file, sizeof(config->output_file), value);
        } else if (strcmp(key, "input_directory") == 0) {
            copy_string(config->input_directory, sizeof(config->input_directory), value);
        } else if (strcmp(key, "output_directory") == 0) {
            copy_string(config->output_directory, sizeof(config->output_directory), value);
        }
    }

    fclose(file);
    return 0;
}

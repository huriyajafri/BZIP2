#include "stages.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

static int g_enabled = 0;
static char g_base[512] = "stages";
static char g_log_path[1024] = "";
static FILE *g_log = NULL;
static char g_direction[32] = "";

static void stage_tag_from_path(const char *path, char *out, size_t outsz)
{
    const char *base = path;
    const char *s;
    char tmp[512];
    size_t n;

    for (s = path; *s; s++) {
        if (*s == '/' || *s == '\\')
            base = s + 1;
    }
    snprintf(tmp, sizeof(tmp), "%s", base);
    for (;;) {
        n = strlen(tmp);
        if (n > 4 && strcmp(tmp + n - 4, ".bz2") == 0) {
            tmp[n - 4] = '\0';
            continue;
        }
        if (n > 4 && strcmp(tmp + n - 4, ".bzp") == 0) {
            tmp[n - 4] = '\0';
            continue;
        }
        if (n > 8 && strcmp(tmp + n - 8, ".bz2impl") == 0) {
            tmp[n - 8] = '\0';
            continue;
        }
        if (n > 9 && strcmp(tmp + n - 9, ".restored") == 0) {
            tmp[n - 9] = '\0';
            continue;
        }
        break;
    }
    snprintf(out, outsz, "%s", tmp);
    {
        char *dot = strrchr(out, '.');
        if (dot)
            *dot = '\0';
    }
}

static int ensure_dir_recursive(const char *path)
{
#ifndef _WIN32
    char cmd[900];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", path);
    return system(cmd);
#else
    char tmp[768];
    size_t i;

    snprintf(tmp, sizeof(tmp), "%s", path);
    for (i = 0; tmp[i] != '\0'; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char saved = tmp[i];
            tmp[i] = '\0';
            if (tmp[0] != '\0')
                MKDIR(tmp);
            tmp[i] = saved;
        }
    }
    return MKDIR(tmp);
#endif
}

static void write_ascii_line(FILE *f, const unsigned char *data, size_t len)
{
    size_t i;
    fputs("ASCII:\n", f);
    for (i = 0; i < len; i++) {
        unsigned char ch = data[i];
        if (ch >= 32 && ch <= 126)
            fputc(ch, f);
        else
            fputc('.', f);
    }
    fputc('\n', f);
}

void stages_configure(int enabled, const char *base_directory)
{
    g_enabled = enabled;
    if (base_directory && base_directory[0] != '\0')
        snprintf(g_base, sizeof(g_base), "%s", base_directory);
}

void stages_begin_run(const char *direction, const char *input_path,
                      const char *output_path, const Config *cfg, int num_blocks)
{
    char stem[256];
    char run_dir[768];

    if (g_log) {
        fclose(g_log);
        g_log = NULL;
    }
    g_log_path[0] = '\0';

    if (!g_enabled || !direction || !input_path || !cfg)
        return;

    snprintf(g_direction, sizeof(g_direction), "%s", direction);
    stage_tag_from_path(input_path, stem, sizeof(stem));
    snprintf(run_dir, sizeof(run_dir), "%s/%s/%s", g_base, stem, direction);
    ensure_dir_recursive(run_dir);
    snprintf(g_log_path, sizeof(g_log_path), "%s/pipeline.txt", run_dir);

    g_log = fopen(g_log_path, "w");
    if (!g_log) {
        fprintf(stderr, "  [stages] Warning: could not open '%s'\n", g_log_path);
        return;
    }

    if (strcmp(direction, "compress") == 0)
        fputs("========== COMPRESSION ==========\n", g_log);
    else
        fputs("========== DECOMPRESSION ==========\n", g_log);

    fprintf(g_log, "INPUT_FILE: %s\n", input_path);
    fprintf(g_log, "OUTPUT_FILE: %s\n", output_path);
    fprintf(g_log, "BLOCK_SIZE: %zu\n", cfg->block_size);
    fprintf(g_log, "NUM_BLOCKS: %d\n", num_blocks);
    fprintf(g_log, "PIPELINE: RLE1=%d BWT=%d MTF=%d RLE2=%d HUFFMAN=%d\n\n",
            cfg->rle1_enabled, cfg->bwt_enabled, cfg->mtf_enabled,
            cfg->rle2_enabled, cfg->huffman_enabled);

    printf("  [stages] Logging -> %s\n", g_log_path);
}

void stages_begin_block(int block_index)
{
    if (!g_log)
        return;
    if (strcmp(g_direction, "compress") == 0)
        fprintf(g_log, "\n######## ENCODE BLOCK %d ########\n\n", block_index);
    else
        fprintf(g_log, "\n######## DECODE BLOCK %d ########\n\n", block_index);
}

void stages_log(const char *stage_name, const unsigned char *data, size_t len)
{
    size_t i;

    if (!g_log || !stage_name)
        return;
    if (!data && len > 0)
        return;

    fputs("----------------------------------------\n", g_log);
    fprintf(g_log, "STAGE: %s\n", stage_name);
    fprintf(g_log, "LENGTH: %zu bytes\n", len);
    fputs("HEX:\n", g_log);

    for (i = 0; i < len; i++) {
        fprintf(g_log, "%02X", data[i]);
        if ((i + 1) % 16 == 0)
            fputc('\n', g_log);
        else if (i + 1 < len)
            fputc(' ', g_log);
    }
    if (len % 16 != 0)
        fputc('\n', g_log);
    fputc('\n', g_log);

    if (len > 0)
        write_ascii_line(g_log, data, len);
    fputc('\n', g_log);
}

void stages_log_value(const char *stage_name, int value)
{
    if (!g_log || !stage_name)
        return;
    fputs("----------------------------------------\n", g_log);
    fprintf(g_log, "STAGE: %s\n", stage_name);
    fprintf(g_log, "VALUE: %d\n\n", value);
}

void stages_end_run(void)
{
    if (g_log) {
        fputs("\n========== END ==========\n", g_log);
        fclose(g_log);
        g_log = NULL;
    }
}

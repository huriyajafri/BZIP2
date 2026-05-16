#ifndef STAGES_H
#define STAGES_H

#include <stddef.h>
#include "config.h"

void stages_configure(int enabled, const char *base_directory);

void stages_begin_run(const char *direction, const char *input_path,
                      const char *output_path, const Config *cfg, int num_blocks);

void stages_begin_block(int block_index);

void stages_log(const char *stage_name, const unsigned char *data, size_t len);

void stages_log_value(const char *stage_name, int value);

void stages_end_run(void);

#endif

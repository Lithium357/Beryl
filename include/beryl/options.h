#ifndef BERYL_OPTIONS_H
#define BERYL_OPTIONS_H
#include "beryl/beryl.h"
int config_option(Config *c, const char *flag, const char *value);
int config_resolve(Config *c, char error[512]);
int parse_u64(const char *text, uint64_t max, uint64_t *value);
int new_directory(const char *path);
#endif

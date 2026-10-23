#ifndef BERYL_GENERATOR_H
#define BERYL_GENERATOR_H
#include "beryl/copybook.h"
#include <stdint.h>
uint64_t generator_next(uint64_t *state);
int generate_record(const Layout *layout, size_t index, uint64_t *state,
                    unsigned char *record, char error[512]);
int generate_cli(int argc, char **argv);
#endif

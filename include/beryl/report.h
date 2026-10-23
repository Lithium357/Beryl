#ifndef BERYL_REPORT_H
#define BERYL_REPORT_H
#include "beryl/beryl.h"
#include <stdio.h>
void json_string(FILE *stream, const char *bytes);
int save_reports(const Config *config, const Result *result, int directory);
#endif

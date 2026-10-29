#ifndef BERYL_COMPILER_ADAPTER_H
#define BERYL_COMPILER_ADAPTER_H
#include "beryl/beryl.h"
int find_tool(const char *name,char path[B_PATH],char error[512]);
int tool_run(const char *tool,const char *const args[],size_t count,const char *cwd,Result *result,char error[512]);
int preprocess_cli(int argc,char **argv);
int coverage_cli(int argc,char **argv);
int instrument_cli(int argc,char **argv);
#endif

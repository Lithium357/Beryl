#ifndef BERYL_CAMPAIGN_H
#define BERYL_CAMPAIGN_H
#include "beryl/copybook.h"
#include "beryl/beryl.h"
int fuzz_cli(int argc,char **argv);
int minimize_cli(int argc,char **argv);
int mutate_record(const Layout *layout,const Bytes *seed,const char *mode,size_t index,uint64_t *state,Bytes *record,char operation[256],char error[512]);
void failure_signature(const Result *r,char hash[65]);
bool failure_equivalent(const Result *a,const Result *b,bool signature);
uint64_t monotonic_ms(void);
#endif

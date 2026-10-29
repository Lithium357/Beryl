#ifndef BERYL_FIXTURES_H
#define BERYL_FIXTURES_H
#include "beryl/beryl.h"
#include <stdio.h>
int fixtures_option(Config *c,const char *flag,const char *value);
int fixtures_validate(const Config *c,char error[512]);
int fixtures_stage(const Config *c,Result *r,int archive,int work,size_t primary,Bytes expected[B_EXTRA_FILES],char error[512]);
int fixtures_capture(const Config *c,Result *r,int work,char error[512]);
void fixtures_golden(const Config *c,Result *r,const Bytes expected[B_EXTRA_FILES]);
int fixtures_reference(const Config *c,Result *r,const Result *reference);
int fixtures_save(const Config *c,const Result *r,int archive);
void fixtures_manifest(FILE *f,const Config *c,const Result *r);
int fixtures_parse(Config *c,const char *key,const char *value);
int fixtures_replay(Config *c,int directory,const char *root,size_t primary);
void fixtures_json(FILE *f,const Config *c,const Result *r);
int fixtures_result_field(Result *r,const char *key,const char *value);
int fixtures_saved_capture(int directory,size_t cap,Result *r);
#endif

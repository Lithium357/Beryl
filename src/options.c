#include "beryl/options.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int parse_u64(const char *s,uint64_t max,uint64_t *out) {
    if(!*s)return -1;
    uint64_t n=0;
    for(;*s;s++){if(*s<'0'||*s>'9')return -1;unsigned d=(unsigned)(*s-'0');if(n>max/10||(n==max/10&&d>max%10))return -1;n=n*10+d;}
    *out=n;return 0;
}
static int string(char *out,size_t capacity,const char *s) {if(strlen(s)>=capacity)return -1;strcpy(out,s);return 0;}
int config_option(Config *c,const char *flag,const char *v) {
    if(!strcmp(flag,"--exec"))return string(c->target,sizeof c->target,v);
    if(!strcmp(flag,"--input"))return string(c->input,sizeof c->input,v);
    if(!strcmp(flag,"--expected")){c->has_expected=true;return string(c->expected,sizeof c->expected,v);}
    if(!strcmp(flag,"--reference"))return string(c->reference,sizeof c->reference,v);
    if(!strcmp(flag,"--results"))return string(c->results,sizeof c->results,v);
    if(!strcmp(flag,"--input-name"))return string(c->input_name,sizeof c->input_name,v);
    if(!strcmp(flag,"--output-name"))return string(c->output_name,sizeof c->output_name,v);
    if(!strcmp(flag,"--compiler-version"))return string(c->compiler,sizeof c->compiler,v);
    if(!strcmp(flag,"--arg")){if(c->argc>=B_ARGS||strlen(v)>=B_PATH)return -1;c->args[c->argc]=strdup(v);if(!c->args[c->argc])return -1;c->argc++;return 0;}
    if(!strcmp(flag,"--compare")){if(!strcmp(v,"output"))c->comparison=0;else if(!strcmp(v,"status"))c->comparison=1;else if(!strcmp(v,"all"))c->comparison=2;else return -1;return 0;}
    if(!strcmp(flag,"--normalize-crlf")){if(!strcmp(v,"0"))c->normalize_crlf=false;else if(!strcmp(v,"1"))c->normalize_crlf=true;else return -1;return 0;}
    const char *keys[]={"--expected-exit","--timeout-ms","--cpu-seconds","--max-input-bytes","--max-capture-bytes","--max-file-bytes"};
    for(unsigned j=0;j<6;j++)if(!strcmp(flag,keys[j])){
        uint64_t n,max=j==0?255U:j==1?600000U:j==2?3600U:67108864U;
        if(parse_u64(v,max,&n)<0||(j&&!n))return -1;
        if(j==0)c->expected_exit=(int)n;else if(j==1)c->timeout_ms=(unsigned)n;else if(j==2)c->cpu_seconds=(unsigned)n;else if(j==3)c->max_input=(size_t)n;else if(j==4)c->max_capture=(size_t)n;else c->max_file=(size_t)n;
        return 0;
    }
    return -2;
}
int config_resolve(Config *c,char error[512]) {
    if(!*c->target){strcpy(error,"--exec is required");return -1;}
    if(!safe_name(c->input_name)||!safe_name(c->output_name)||!strcmp(c->input_name,c->output_name)){strcpy(error,"--input-name/--output-name require distinct safe relative paths");return -1;}
    if(c->has_expected&&*c->reference){strcpy(error,"--expected and --reference are alternative oracles");return -1;}
    if(!*c->reference&&(c->comparison||c->normalize_crlf)){strcpy(error,"comparison policy requires --reference");return -1;}
    char resolved[B_PATH];
    if(!realpath(c->target,resolved)){snprintf(error,512,"--exec %.300s: %s",c->target,strerror(errno));return -1;}strcpy(c->target,resolved);
    if(*c->reference){if(!realpath(c->reference,resolved)){snprintf(error,512,"--reference %.300s: %s",c->reference,strerror(errno));return -1;}strcpy(c->reference,resolved);}
    return 0;
}

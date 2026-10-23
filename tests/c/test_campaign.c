#include "beryl/campaign.h"
#include "beryl/record_codec.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int invoke(const char *cli,char *const args[]) {
    pid_t p=fork();assert(p>=0);if(!p){int fd=open("/dev/null",O_WRONLY);assert(fd>=0);assert(dup2(fd,1)>=0);execv(cli,args);_exit(99);}
    int s;while(waitpid(p,&s,0)<0)assert(errno==EINTR);assert(WIFEXITED(s));return WEXITSTATUS(s);
}
static void put(const char *path,const char *text){FILE *f=fopen(path,"wb");assert(f);assert(fwrite(text,1,strlen(text),f)==strlen(text));assert(!fclose(f));}
static char *load(const char *path){Bytes b={0};assert(!read_bytes(path,B_ENV_LIMIT,&b));b.data[b.len]=0;return (char *)b.data;}
int main(int argc,char **argv) {
    assert(argc==4);char cli[B_PATH];assert(realpath(argv[1],cli));char base[]="/tmp/beryl-campaign-XXXXXX";assert(mkdtemp(base));
    Layout layout;char error[512],op[256];const char *schema="01 r. 05 f PIC S9(3) COMP-3. 05 t PIC X(3).";assert(!copybook_parse(schema,strlen(schema),&layout,error));
    Bytes empty={0};uint64_t state=123;
    for(size_t i=0;i<100;i++){Bytes b={0};assert(!mutate_record(&layout,&empty,"valid",i,&state,&b,op,error));assert(!record_validate(&layout,b.data,b.len,error));bytes_free(&b);assert(!mutate_record(&layout,&empty,"invalid",i,&state,&b,op,error));assert(record_validate(&layout,b.data,b.len,error)<0);bytes_free(&b);}
    Result a={0},b={0};a.outcome=CRASH;a.signal_number=11;b=a;assert(failure_equivalent(&a,&b,false));b.signal_number=6;assert(!failure_equivalent(&a,&b,false));
    a.outcome=FAIL_ASSERTION;a.signal_number=0;strcpy(a.diagnostic,"missing required output");b=a;strcpy(b.diagnostic,"binary difference");assert(!failure_equivalent(&a,&b,false));
    char seed[B_PATH],cpy[B_PATH],one[B_PATH],two[B_PATH];assert(!path_join(seed,base,"seed"));assert(!path_join(cpy,base,"layout.cpy"));assert(!path_join(one,base,"one"));assert(!path_join(two,base,"two"));put(seed,"D123456");put(cpy,"01 r. 05 f PIC 9(3).");
    char *args[]={cli,"fuzz","--exec",argv[3],"--reference",argv[2],"--input",seed,"--mode","raw","--runs","8","--seed","42","--results",one,NULL};
    assert(invoke(cli,args)==1);args[15]=two;assert(invoke(cli,args)==1);
    for(size_t i=0;i<8;i++){char file[64],x[B_PATH],y[B_PATH];snprintf(file,sizeof file,"run-%06zu/input.bin",i);assert(!path_join(x,one,file));assert(!path_join(y,two,file));Bytes bx={0},by={0};assert(!read_bytes(x,100,&bx));assert(!read_bytes(y,100,&by));assert(bx.len==by.len&&!memcmp(bx.data,by.data,bx.len));bytes_free(&bx);bytes_free(&by);}
    char manifest[B_PATH];assert(!path_join(manifest,one,"campaign.json"));char *json=load(manifest);assert(strstr(json,"\"completed_runs\":8")&&strstr(json,"REGRESSION_CANDIDATE")&&strstr(json,"\"reproducible\":true"));free(json);
    char valid[B_PATH];assert(!path_join(valid,base,"valid"));char *observe[]={cli,"fuzz","--exec",argv[2],"--copybook",cpy,"--runs","6","--seed","0","--results",valid,NULL};assert(!invoke(cli,observe));assert(!path_join(manifest,valid,"campaign.json"));json=load(manifest);assert(strstr(json,"UNVERIFIED")&&!strstr(json,"\"classification\":\"PASS\""));free(json);
    char bounded[B_PATH];assert(!path_join(bounded,base,"bounded"));args[15]=bounded;args[16]="--max-total-bytes";char *bounded_args[20];for(size_t i=0;i<17;i++)bounded_args[i]=args[i];bounded_args[17]="7";bounded_args[18]=NULL;assert(invoke(cli,bounded_args)==2);assert(!path_join(manifest,bounded,"campaign.json"));json=load(manifest);assert(strstr(json,"artifact-budget")&&strstr(json,"\"completed_runs\":0"));free(json);
    printf("campaign: deterministic mutation, valid/invalid storage, differential failures, reproduction, grouping, unverified observations and artifact budget passed\nevidence: %s\n",base);return 0;
}

#include "beryl/beryl.h"
#include "beryl/copybook.h"
#include "beryl/record_codec.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int invoke(char *const args[]) {
    pid_t p=fork();assert(p>=0);if(!p){int fd=open("/dev/null",O_WRONLY);assert(fd>=0);assert(dup2(fd,1)>=0);execv(args[0],args);_exit(99);}
    int s;while(waitpid(p,&s,0)<0)assert(errno==EINTR);assert(WIFEXITED(s));return WEXITSTATUS(s);
}
static void put(const char *path,const char *text) {FILE *f=fopen(path,"wb");assert(f);assert(fwrite(text,1,strlen(text),f)==strlen(text));assert(!fclose(f));}
static Bytes read_file(const char *root,const char *name) {char path[B_PATH];assert(!path_join(path,root,name));Bytes bytes={0};assert(!read_bytes(path,B_ENV_LIMIT,&bytes));return bytes;}
int main(int argc,char **argv) {
    assert(argc==3);char cli[B_PATH],target[B_PATH];assert(realpath(argv[1],cli));assert(realpath(argv[2],target));
    char root[]="/tmp/beryl-minimizer-XXXXXX";assert(mkdtemp(root));char input[B_PATH],expected[B_PATH],results[B_PATH],out[B_PATH],schema[B_PATH];
    assert(!path_join(input,root,"input"));assert(!path_join(expected,root,"expected"));assert(!path_join(results,root,"results"));assert(!path_join(out,root,"reduced"));assert(!path_join(schema,root,"layout.cpy"));
    put(input,"BABCDEFGHIJ");put(expected,"XYZ");put(schema,"01 r. 05 key-field pic X. 05 payload pic X(10).");
    Config c;config_defaults(&c);strcpy(c.target,target);strcpy(c.input,input);strcpy(c.expected,expected);strcpy(c.results,results);c.has_expected=true;
    Result original;assert(!execute_case(&c,&original));assert(original.outcome==FAIL_ASSERTION);
    char saved[B_PATH];strcpy(saved,original.directory);result_free(&original);config_free(&c);
    char *args[]={cli,"minimize",saved,"--out",out,"--max-tries","100","--budget-ms","30000",NULL,NULL,NULL};assert(!invoke(args));
    Bytes reduced=read_file(out,"minimized.bin");assert(reduced.len==1);bytes_free(&reduced);
    Bytes archived=read_file(out,"original.bin");assert(archived.len==11&&!memcmp(archived.data,"BABCDEFGHIJ",11));bytes_free(&archived);
    char structured[B_PATH];assert(!path_join(structured,root,"structured"));args[4]=structured;args[9]="--copybook";args[10]=schema;assert(!invoke(args));
    reduced=read_file(structured,"minimized.bin");Layout layout;char error[512];assert(!copybook_load(schema,&layout,error));assert(!record_validate(&layout,reduced.data,reduced.len,error));assert(reduced.len==11);for(size_t i=0;i<reduced.len;i++)assert(reduced.data[i]==' ');bytes_free(&reduced);
    char strict[B_PATH];assert(!path_join(strict,root,"strict"));args[4]=strict;args[9]="--predicate";args[10]="signature";assert(!invoke(args));reduced=read_file(strict,"minimized.bin");assert(reduced.len==11);bytes_free(&reduced);
    char limited[B_PATH];assert(!path_join(limited,root,"limited"));args[4]=limited;args[6]="1";args[9]=NULL;assert(!invoke(args));reduced=read_file(limited,"minimized.bin");assert(reduced.len==11);bytes_free(&reduced);Bytes report=read_file(limited,"minimization.json");report.data[report.len]=0;assert(strstr((char *)report.data,"try-limit"));bytes_free(&report);
    /* A changed approved baseline must not silently become a different failure. */
    char tampered[B_PATH];assert(!path_join(tampered,saved,"expected.bin"));put(tampered,"changed");assert(invoke(args)==2);
    printf("minimizer: raw and structure-preserving reduction, signature predicate, bounded trials, original preservation and archive integrity passed\nevidence: %s\n",root);return 0;
}

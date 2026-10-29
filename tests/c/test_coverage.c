#include "beryl/beryl.h"
#include "beryl/compiler_adapter.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int invoke(char *const argv[]){pid_t p=fork();assert(p>=0);if(!p){execv(argv[0],argv);_exit(99);}int s;while(waitpid(p,&s,0)<0)assert(errno==EINTR);assert(WIFEXITED(s));return WEXITSTATUS(s);}
static void put(const char *path,const char *text){FILE *f=fopen(path,"wb");assert(f);assert(fwrite(text,1,strlen(text),f)==strlen(text));assert(!fclose(f));}
static char *report(const char *root){char path[B_PATH];assert(!path_join(path,root,"coverage.json"));Bytes b={0};assert(!read_bytes(path,B_ENV_LIMIT,&b));b.data[b.len]=0;return (char *)b.data;}
int main(int argc,char **argv) {
    assert(argc==4);char error[512],gcov[B_PATH],cli[B_PATH],source[B_PATH],root[]="/tmp/beryl-coverage-XXXXXX";assert(realpath(argv[1],cli));assert(realpath(argv[3],source));
    if(find_tool("gcov",gcov,error)<0){puts("SKIP: gcov not installed");return 77;}
    assert(mkdtemp(root));Result version={0};const char *v[]={"--dumpversion"};assert(!tool_run(argv[2],v,1,root,&version,error));version.out.data[version.out.len]=0;if(strncmp((char *)version.out.data,"3.2",3)){result_free(&version);puts("SKIP: mapping adapter currently validated for GnuCOBOL 3.2 only");return 77;}result_free(&version);
    char build[B_PATH],one[B_PATH],two[B_PATH],input[B_PATH],expected[B_PATH],results[B_PATH],program[B_PATH];assert(!path_join(build,root,"build"));assert(!path_join(one,root,"one"));assert(!path_join(two,root,"two"));assert(!path_join(input,root,"input"));assert(!path_join(expected,root,"expected"));assert(!path_join(results,root,"results"));assert(!path_join(program,build,"program"));
    char *instrument[]={cli,"instrument","--source",source,"--out",build,"--cobc",argv[2],NULL};assert(!invoke(instrument));
    char *collect[]={cli,"coverage","--build",build,"--out",one,"--gcov",gcov,NULL};assert(invoke(collect)==2);
    put(input,"100");put(expected,"100");char *run[]={cli,"run","--exec",program,"--input",input,"--expected",expected,"--results",results,NULL};assert(!invoke(run));assert(!invoke(collect));char *json=report(one);assert(strstr(json,"\"line\":23,\"count\":1")&&strstr(json,"\"line\":25,\"count\":0"));free(json);
    put(input,"099");put(expected,"099");assert(!invoke(run));collect[5]=two;assert(!invoke(collect));json=report(two);assert(strstr(json,"\"line\":25,\"count\":1"));free(json);
    char tampered[B_PATH];assert(!path_join(tampered,build,"source.cob"));put(tampered,"changed");assert(invoke(collect)==2);
    printf("coverage: real GnuCOBOL/GCC mapping, known hit/miss branches, cumulative execution and changed-source refusal passed\nevidence: %s\n",root);return 0;
}

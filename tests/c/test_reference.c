#include "beryl/beryl.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static char cli[B_PATH],base[B_PATH];
static int invoke(char *const args[]) {
    pid_t p=fork();assert(p>=0);if(!p){int fd=open("/dev/null",O_WRONLY);assert(fd>=0);assert(dup2(fd,1)>=0);execv(cli,args);_exit(99);}
    int s;while(waitpid(p,&s,0)<0)assert(errno==EINTR);assert(WIFEXITED(s));return WEXITSTATUS(s);
}
static void one_case(const char *candidate,const char *reference,const char *name,const char *input,const char *policy,const char *normalize,const char *classification,int expected) {
    char root[B_PATH],source[B_PATH];assert(!path_join(root,base,name));assert(!path_join(source,base,"input"));
    FILE *f=fopen(source,"wb");assert(f);assert(fwrite(input,1,strlen(input),f)==strlen(input));assert(!fclose(f));
    char *args[]={cli,"run","--exec",(char *)candidate,"--reference",(char *)reference,"--input",source,"--results",root,"--compare",(char *)policy,"--normalize-crlf",(char *)normalize,"--timeout-ms","500",NULL};
    assert(invoke(args)==expected);DIR *d=opendir(root);assert(d);struct dirent *e;char result[B_PATH]="";
    while((e=readdir(d)))if(!strncmp(e->d_name,"case-",5))assert(!path_join(result,root,e->d_name));
    closedir(d);assert(*result);
    int fd=open_directory(result,false);assert(fd>=0);Bytes manifest={0};assert(!read_at(fd,"manifest.txt",B_ENV_LIMIT,&manifest));manifest.data[manifest.len]=0;
    char check[128];snprintf(check,sizeof check,"classification=%s\n",classification);
    if(!strstr((char *)manifest.data,check))fprintf(stderr,"%s: expected %s\n%s",name,classification,manifest.data);
    assert(strstr((char *)manifest.data,check));bytes_free(&manifest);close(fd);
    if(!strcmp(name,"difference")) {
        char replay[B_PATH];assert(!path_join(replay,base,"replay"));char *again[]={cli,"replay",result,"--results",replay,NULL};assert(invoke(again)==1);
    }
    printf("reference: %s => %s\n",name,classification);
}
int main(int argc,char **argv) {
    assert(argc==4&&realpath(argv[1],cli));strcpy(base,"/tmp/beryl-reference-XXXXXX");assert(mkdtemp(base));
    one_case(argv[2],argv[2],"equal","D123","output","0","PASS",0);
    one_case(argv[3],argv[2],"difference","D123","output","0","REGRESSION_CANDIDATE",1);
    one_case(argv[3],argv[2],"status-only","D123","status","0","PASS",0);
    one_case(argv[3],argv[2],"status-difference","E123","output","0","REGRESSION_CANDIDATE",1);
    one_case(argv[3],argv[2],"equal-nonzero","N123","output","0","PASS",0);
    one_case(argv[3],argv[2],"streams-ignored","S123","output","0","PASS",0);
    one_case(argv[3],argv[2],"streams-compared","S123","all","0","REGRESSION_CANDIDATE",1);
    one_case(argv[3],argv[2],"crash","C123","output","0","REGRESSION_CANDIDATE",1);
    one_case(argv[3],argv[2],"timeout","T123","output","0","TIMEOUT",1);
    one_case(argv[3],argv[2],"missing","M123","output","0","REGRESSION_CANDIDATE",1);
    one_case(argv[3],argv[2],"raw-crlf","L123","output","0","REGRESSION_CANDIDATE",1);
    one_case(argv[3],argv[2],"normalized-crlf","L123","output","1","PASS",0);
    return 0;
}

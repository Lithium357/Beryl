#include "beryl/beryl.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int invoke(char *const argv[]) {
    pid_t p=fork();assert(p>=0);if(!p){execv(argv[0],argv);_exit(99);}int s;while(waitpid(p,&s,0)<0)assert(errno==EINTR);assert(WIFEXITED(s));return WEXITSTATUS(s);
}
static void put(const char *path,const char *text){FILE *f=fopen(path,"wb");assert(f);assert(fwrite(text,1,strlen(text),f)==strlen(text));assert(!fclose(f));}
int main(int argc,char **argv) {
    assert(argc==2||(argc>=4&&argc<=20));char cli[B_PATH],root[]="/tmp/beryl-driver-XXXXXX",out[B_PATH],source[B_PATH];assert(realpath(argv[1],cli));assert(mkdtemp(root));assert(!path_join(out,root,"generated"));assert(!path_join(source,out,"driver.cob"));
    char *generate[]={cli,"driver","--program","CLAMPREC","--record-bytes","3","--out",out,NULL};assert(!invoke(generate));Bytes text={0};assert(!read_bytes(source,B_ENV_LIMIT,&text));text.data[text.len]=0;assert(strstr((char *)text.data,"using by reference call-record"));bytes_free(&text);assert(invoke(generate)==2);
    generate[3]="bad\"name";assert(invoke(generate)==2);generate[3]="CLAMPREC";generate[5]="0";assert(invoke(generate)==2);generate[5]="65537";assert(invoke(generate)==2);
    if(argc>=4){
        char program[B_PATH],subprogram[B_PATH],input[B_PATH],expected[B_PATH],results[B_PATH];assert(!path_join(program,root,"call-test"));assert(realpath(argv[3],subprogram));assert(!path_join(input,root,"input"));assert(!path_join(expected,root,"expected"));assert(!path_join(results,root,"results"));
        if(!strcmp(argv[2],"--prebuilt")) {assert(argc==4);assert(realpath(argv[3],program));}
        else {
        char *compile[24]={argv[2]};size_t n=1;
        if(argc==4){compile[n++]="-x";compile[n++]="-free";}
        else for(int i=4;i<argc;i++)compile[n++]=argv[i];
        compile[n++]="-o";compile[n++]=program;compile[n++]=source;compile[n++]=subprogram;compile[n]=NULL;
        assert(!invoke(compile));
        }
        put(input,"099");put(expected,"100");
        char *run[]={cli,"run","--exec",program,"--input",input,"--expected",expected,"--expected-exit","7","--results",results,NULL};assert(!invoke(run));
        put(input,"100");assert(!invoke(run));put(input,"100100");assert(invoke(run)==1);
        puts("COBOL driver: explicit linkage call, record transformation, return-code preservation and extra-record refusal passed");
    }
    printf("driver: generation, exclusive output, invalid name/width checks passed\nevidence: %s\n",root);return 0;
}

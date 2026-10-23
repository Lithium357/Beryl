#include "beryl/beryl.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static char suite[B_PATH], cli[B_PATH], target[B_PATH];
static unsigned tests=0;
static void put(const char *path,const void *data,size_t len) {
    FILE *f=fopen(path,"wb");assert(f);assert(fwrite(data,1,len,f)==len);assert(!fclose(f));
}
static int invoke(char *const argv[]) {
    pid_t pid=fork();assert(pid>=0);
    if(!pid){char log[B_PATH];assert(!path_join(log,suite,"cli.log"));int fd=open(log,O_WRONLY|O_CREAT|O_TRUNC,0600);assert(fd>=0);assert(dup2(fd,1)>=0&&dup2(fd,2)>=0);close(fd);execv(cli,argv);_exit(99);}
    int status;while(waitpid(pid,&status,0)<0)assert(errno==EINTR);
    if(!WIFEXITED(status)){fprintf(stderr,"CLI terminated by signal %d\n",WTERMSIG(status));abort();}return WEXITSTATUS(status);
}
static char *load(const char *path) { Bytes b={0};assert(!read_bytes(path,2U*1024U*1024U,&b));assert(!b.truncated);b.data[b.len]=0;return (char *)b.data; }
static char *manifest(const char *dir) { char path[B_PATH];assert(!path_join(path,dir,"manifest.txt"));return load(path); }
static void find_case(const char *root,char result[B_PATH]) {
    DIR *d=opendir(root);assert(d);struct dirent *ent;unsigned n=0;
    while((ent=readdir(d)))if(!strncmp(ent->d_name,"case-",5)){assert(!path_join(result,root,ent->d_name));n++;}
    closedir(d);assert(n==1);
}
static void check_bytes(const char *dir,const char *file,const void *expected,size_t len) {
    char path[B_PATH];assert(!path_join(path,dir,file));Bytes b={0};assert(!read_bytes(path,2U*1024U*1024U,&b));assert(b.len==len&&!b.truncated&&!memcmp(b.data,expected,len));bytes_free(&b);
}
static void run_case(const char *label,unsigned char mode,const void *bytes,size_t len,const char *classification,int status,char result[B_PATH],const char *const *extra) {
    char input[B_PATH],expected[B_PATH],root[B_PATH];assert(!path_join(input,suite,"input"));assert(!path_join(expected,suite,"expected"));assert(!path_join(root,suite,label));
    unsigned char *data=malloc(len+1);assert(data);data[0]=mode;memcpy(data+1,bytes,len);put(input,data,len+1);free(data);put(expected,bytes,len);
    char *args[80]={cli,"run","--exec",target,"--input",input,"--expected",expected,"--results",root};size_t n=10;
    if(extra)for(size_t i=0;extra[i];i++)args[n++]=(char *)extra[i];
    args[n]=NULL;
    int actual=invoke(args);
    if(actual!=status){char log[B_PATH];assert(!path_join(log,suite,"cli.log"));char *s=load(log);fprintf(stderr,"%s: expected status %d actual %d\n%s",label,status,actual,s);free(s);abort();}
    find_case(root,result);char *m=manifest(result);char key[100];snprintf(key,sizeof key,"classification=%s\n",classification);assert(strstr(m,key));free(m);
    tests++;printf("ok %u: %s (%s)\n",tests,label,classification);
}
static void replay_case(const char *original,const char *label,int status,char result[B_PATH]) {
    char root[B_PATH];assert(!path_join(root,suite,label));char *args[]={cli,"replay",(char *)original,"--results",root,NULL};assert(invoke(args)==status);find_case(root,result);
}
static void cobol_tests(const char *good,const char *bug,const char *source) {
    char input[B_PATH],expected[B_PATH],root[B_PATH],result[B_PATH];assert(!path_join(input,source,"tests/records/boundary.bin"));assert(!path_join(expected,source,"tests/expected/boundary.out"));
    assert(!path_join(root,suite,"cobol-pass"));
    char *args[]={cli,"run","--exec",(char *)good,"--input",input,"--expected",expected,"--results",root,NULL};assert(!invoke(args));find_case(root,result);char *m=manifest(result);assert(strstr(m,"classification=PASS\n"));free(m);check_bytes(result,"output.bin","100",3);tests++;
    char original[B_PATH];strcpy(original,result);replay_case(original,"cobol-replay",0,result);check_bytes(result,"output.bin","100",3);tests++;
    assert(!path_join(root,suite,"cobol-bug"));args[3]=(char *)bug;assert(invoke(args)==1);find_case(root,result);m=manifest(result);assert(strstr(m,"classification=FAIL_ASSERTION\n"));free(m);check_bytes(result,"output.bin","101",3);tests++;
    printf("COBOL: %u checks passed (fixed SEQUENTIAL golden, replay, boundary bug)\n",tests);
}
int main(int argc,char **argv) {
    if(argc==2&&!strcmp(argv[1],"--skip-cobol")){puts("SKIP: GnuCOBOL compiler unavailable");return 77;}
    assert(argc==3||argc==5);assert(realpath(argv[1],cli));
    strcpy(suite,"/tmp/beryl-acceptance-XXXXXX");assert(mkdtemp(suite));printf("evidence: %s\n",suite);
    if(argc==5){cobol_tests(argv[2],argv[3],argv[4]);return 0;}
    assert(realpath(argv[2],target));
    const unsigned char bytes[]={0,1,255,0,128,'\n'};char result[B_PATH],original[B_PATH];
    run_case("pass-binary",'B',bytes,sizeof bytes,"PASS",0,result,NULL);check_bytes(result,"output.bin",bytes,sizeof bytes);strcpy(original,result);
    replay_case(original,"replay",0,result);check_bytes(result,"output.bin",bytes,sizeof bytes);
    char *a=manifest(original),*b=manifest(result);char *ah=strstr(a,"input_sha256="),*bh=strstr(b,"input_sha256=");assert(ah&&bh&&!memcmp(ah,bh,77));free(a);free(b);tests++;puts("ok: replay input digest and binary result identical");
    run_case("missing",'M',bytes,sizeof bytes,"FAIL_ASSERTION",1,result,NULL);a=manifest(result);assert(strstr(a,"missing_output=1\n"));free(a);
    char log_path[B_PATH];assert(!path_join(log_path,suite,"cli.log"));a=load(log_path);assert(strstr(a,"missing required output file: OUTPUT.DAT"));free(a);
    run_case("nonzero",'N',bytes,sizeof bytes,"FAIL_ASSERTION",1,result,NULL);
    const char *exit7[]={"--expected-exit","7",NULL};run_case("expected-seven",'N',bytes,sizeof bytes,"PASS",0,result,exit7);
    run_case("segfault",'C',bytes,sizeof bytes,"CRASH",1,result,NULL);
    run_case("abort",'A',bytes,sizeof bytes,"CRASH",1,result,NULL);
    const char *short_time[]={"--timeout-ms","100",NULL};run_case("timeout-descendants",'T',bytes,sizeof bytes,"TIMEOUT",1,result,short_time);
    struct timespec delay={1,200000000};assert(!nanosleep(&delay,NULL));char marker[B_PATH];assert(!path_join(marker,result,"work/descendant-survived"));assert(access(marker,F_OK)<0&&errno==ENOENT);tests++;puts("ok: descendant did not survive timeout");
    run_case("infinite-cpu",'L',bytes,sizeof bytes,"TIMEOUT",1,result,short_time);
    const char *cpu[]={"--cpu-seconds","1","--timeout-ms","4000",NULL};run_case("cpu-cap",'L',bytes,sizeof bytes,"CRASH",1,result,cpu);
    const char *small[]={"--max-capture-bytes","64",NULL};unsigned char xs[64];memset(xs,'x',sizeof xs);
    run_case("truncation",'O',xs,sizeof xs,"FAIL_ASSERTION",1,result,small);a=manifest(result);assert(strstr(a,"stdout_truncated=1\n")&&strstr(a,"stderr_truncated=1\n")&&strstr(a,"output_truncated=1\n"));free(a);check_bytes(result,"output.bin",xs,64);
    const char *filecap[]={"--max-file-bytes","1024",NULL};run_case("file-cap",'F',xs,sizeof xs,"CRASH",1,result,filecap);
    run_case("symlink-output",'S',bytes,sizeof bytes,"HARNESS_ERROR",2,result,NULL);
    const char *nested[]={"--output-name","nested/OUTPUT.DAT",NULL};run_case("nested-symlink-output",'D',bytes,sizeof bytes,"HARNESS_ERROR",2,result,nested);
    run_case("orphan-cleanup",'P',bytes,sizeof bytes,"FAIL_ASSERTION",1,result,NULL);
    assert(!setenv("BERYL_FIXTURE_VALUE","saved-env:",1));
    const char *envargs[]={"--arg","literal ; $(touch SHOULD_NOT_EXIST)",NULL};
    const char *value="saved-env:literal ; $(touch SHOULD_NOT_EXIST)";
    run_case("environment-argv",'E',value,strlen(value),"PASS",0,result,envargs);strcpy(original,result);
    assert(!setenv("BERYL_FIXTURE_VALUE","changed-env:",1));replay_case(original,"environment-replay",0,result);check_bytes(result,"output.bin",value,strlen(value));tests++;
    const char *help_arg[]={"--arg","--help",NULL};
    run_case("literal-help-argument",'E',"changed-env:--help",18,"PASS",0,result,help_arg);
    char input[B_PATH],root[B_PATH];assert(!path_join(input,suite,"raw"));put(input,"Babc",4);assert(!path_join(root,suite,"unverified"));
    char *plain[]={cli,"run","--exec",target,"--input",input,"--results",root,NULL};assert(!invoke(plain));find_case(root,result);a=manifest(result);assert(strstr(a,"classification=UNVERIFIED\n"));free(a);tests++;
    char noexec[B_PATH];assert(!path_join(noexec,suite,"noexec"));put(noexec,"not executable",14);plain[3]=noexec;assert(!path_join(root,suite,"exec-error"));assert(invoke(plain)==2);find_case(root,result);a=manifest(result);assert(strstr(a,"classification=EXEC_ERROR\n"));free(a);tests++;
    char *invalid[]={cli,"run","--exec",target,"--input",input,"--input-name","../escape","--results",root,NULL};assert(invoke(invalid)==2);invalid[7]="/absolute";assert(invoke(invalid)==2);tests++;
    char link[B_PATH];assert(!path_join(link,suite,"result-link"));assert(!symlink("/tmp",link));plain[3]=target;plain[7]=link;assert(invoke(plain)==2);tests++;
    assert(!path_join(root,suite,"oversized-input"));plain[7]=root;char *too_big[]={cli,"run","--exec",target,"--input",input,"--max-input-bytes","2","--results",root,NULL};assert(invoke(too_big)==2);tests++;
    /* Deliberate archived-byte tampering must be rejected before executing. */
    assert(!path_join(input,original,"input.bin"));put(input,"tampered",8);char *tamper[]={cli,"replay",original,NULL};assert(invoke(tamper)==2);tests++;
    /* A changed target is refused unless the caller explicitly overrides it. */
    char replacement[B_PATH];assert(!path_join(replacement,suite,"replaceable-target"));
    Bytes executable={0};assert(!read_bytes(target,16U*1024U*1024U,&executable));put(replacement,executable.data,executable.len);assert(!chmod(replacement,0700));
    plain[3]=replacement;assert(!path_join(root,suite,"target-original"));plain[7]=root;assert(!path_join(input,suite,"raw"));put(input,"Babc",4);plain[5]=input;
    assert(!invoke(plain));find_case(root,original);
    put(replacement,"changed",7);char *changed[]={cli,"replay",original,NULL};assert(invoke(changed)==2);tests++;
    assert(!path_join(root,suite,"target-override"));char *allow[]={cli,"replay",original,"--exec",target,"--results",root,NULL};assert(!invoke(allow));tests++;
    bytes_free(&executable);
    printf("C acceptance: %u checks passed\n",tests);return 0;
}

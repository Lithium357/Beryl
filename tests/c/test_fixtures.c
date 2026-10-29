#include "beryl/beryl.h"
#include "beryl/options.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static void put(const char *path,const void *bytes,size_t len){FILE *f=fopen(path,"wb");assert(f);assert(fwrite(bytes,1,len,f)==len);assert(!fclose(f));}
static int invoke(char *const argv[]){pid_t p=fork();assert(p>=0);if(!p){execv(argv[0],argv);_exit(99);}int s;while(waitpid(p,&s,0)<0)assert(errno==EINTR);assert(WIFEXITED(s));return WEXITSTATUS(s);}
int main(int argc,char **argv) {
    assert(argc==3);char cli[B_PATH],target[B_PATH],root[]="/tmp/beryl-files-XXXXXX",input[B_PATH],second[B_PATH],golden[B_PATH],results[B_PATH],mapping[B_PATH+512],output[B_PATH+512],error[512];
    assert(realpath(argv[1],cli)&&realpath(argv[2],target));assert(mkdtemp(root));assert(!path_join(input,root,"input"));assert(!path_join(second,root,"second"));assert(!path_join(golden,root,"golden"));assert(!path_join(results,root,"results"));
    const unsigned char raw[]={0,255,1,0};put(input,"primary",7);put(second,raw,sizeof raw);put(golden,raw,sizeof raw);
    snprintf(mapping,sizeof mapping,"CONFIG.DAT=%s",second);snprintf(output,sizeof output,"nested/EXTRA.DAT=%s",golden);
    Config c;config_defaults(&c);strcpy(c.target,target);strcpy(c.input,input);strcpy(c.results,results);assert(!config_option(&c,"--file",mapping));assert(!config_option(&c,"--output",output));assert(!config_resolve(&c,error));
    Result result;assert(!execute_case(&c,&result));assert(result.outcome==PASS&&result.extra_count==1&&result.extra[0].bytes.len==sizeof raw&&!memcmp(result.extra[0].bytes.data,raw,sizeof raw));char saved[B_PATH];strcpy(saved,result.directory);result_free(&result);
    char replay[B_PATH];assert(!path_join(replay,root,"replay"));char *args[]={cli,"replay",saved,"--results",replay,NULL};assert(!invoke(args));
    put(golden,"different",9);assert(!execute_case(&c,&result));assert(result.outcome==FAIL_ASSERTION&&strstr(result.diagnostic,"nested/EXTRA.DAT"));result_free(&result);
    char reduced[B_PATH];assert(!path_join(reduced,root,"reduced"));/* Restore the case with a mismatch only in the extra golden. */
    assert(!execute_case(&c,&result));strcpy(saved,result.directory);result_free(&result);
    char *minimize[]={cli,"minimize",saved,"--out",reduced,"--max-tries","20",NULL};assert(!invoke(minimize));
    c.outputs[0].oracle=false;strcpy(c.reference,target);assert(!execute_case(&c,&result));assert(result.outcome==PASS);result_free(&result);c.reference[0]=0;
    c.output_count=2;strcpy(c.outputs[1].name,"MISSING.DAT");assert(!execute_case(&c,&result));assert(result.outcome==FAIL_ASSERTION&&strstr(result.diagnostic,"MISSING.DAT"));result_free(&result);c.output_count=1;
    put(input,"p",1);c.max_capture=2;strcpy(c.reference,target);assert(!execute_case(&c,&result));assert(result.outcome==HARNESS_ERROR&&strstr(result.diagnostic,"nested/EXTRA.DAT"));result_free(&result);c.reference[0]=0;c.max_capture=1048576;put(input,"primary",7);
    c.max_input=7;assert(execute_case(&c,&result)<0&&result.outcome==HARNESS_ERROR);result_free(&result);c.max_input=1048576;
    strcpy(c.outputs[0].name,"CONFIG.DAT/child");assert(config_resolve(&c,error)<0);strcpy(c.outputs[0].name,"nested/EXTRA.DAT");
    strcpy(c.outputs[0].name,"../escape");assert(config_resolve(&c,error)<0);
    char archived_input[B_PATH];assert(!path_join(archived_input,saved,"input-00.bin"));put(archived_input,"changed",7);args[2]=saved;assert(invoke(args)==2);
    config_free(&c);printf("fixtures: multiple binary inputs/outputs, extra golden oracle, replay, reduction, reference, aggregate input cap, collisions and digest checks passed\nevidence: %s\n",root);return 0;
}

#include "beryl/compiler_adapter.h"
#include "beryl/options.h"
#include "beryl/report.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int write_stream(int dir,const char *name,FILE *f,char **data,size_t *len) {
    int bad=ferror(f);if(fclose(f))bad=1;return bad?-1:write_at(dir,name,*data,*len);
}
int instrument_cli(int argc,char **argv) {
    const char *source=NULL,*out=NULL,*compiler="cobc",*format="free";
    for(int i=2;i<argc;i++){const char *flag=argv[i];if(!strcmp(flag,"--help")){puts("Usage: beryl instrument --source FILE --out NEW-DIR [--cobc PATH] [--format free|fixed]\n  GnuCOBOL 3.2/GCC mapped-line coverage adapter. Build/program is the executable.\n  Run it through beryl run, then beryl coverage --build BUILD-DIR --out NEW-DIR.");return 0;}if(i+1==argc){fprintf(stderr,"error: %s requires a value\n",flag);return 2;}const char *v=argv[++i];if(!strcmp(flag,"--source"))source=v;else if(!strcmp(flag,"--out"))out=v;else if(!strcmp(flag,"--cobc"))compiler=v;else if(!strcmp(flag,"--format"))format=v;else{fprintf(stderr,"error: unknown instrument flag %s\n",flag);return 2;}}
    if(!source||!out||(strcmp(format,"free")&&strcmp(format,"fixed"))){fputs("error: instrument requires --source, --out and format free|fixed\n",stderr);return 2;}
    char error[512]="",tool[B_PATH],root[B_PATH],source_absolute[B_PATH],parent[B_PATH];Bytes input={0},generated={0};Result build={0},version={0};Config env;config_defaults(&env);int dir=-1,rc=2;FILE *f=NULL;char *text=NULL,*marker=NULL;size_t text_len=0,marker_len=0;
    if(find_tool(compiler,tool,error)<0)goto fail;
    if(!realpath(source,source_absolute)||read_bytes(source_absolute,B_ENV_LIMIT,&input)<0||input.truncated){strcpy(error,"cannot read bounded COBOL source");goto fail;}
    dir=new_directory(out);if(dir<0||!realpath(out,root)){strcpy(error,"--out must be new and writable");goto fail;}
    if(copy_to(dir,"source.cob",&input)<0){strcpy(error,"cannot archive source");goto fail;}
    strcpy(parent,source_absolute);char *slash=strrchr(parent,'/');if(slash==parent)parent[1]=0;else *slash=0;
    const char *args[]={"-x",!strcmp(format,"fixed")?"-fixed":"-free","--coverage","--save-temps","-I",parent,"-o","program","source.cob"};
    int built=tool_run(tool,args,sizeof args/sizeof *args,root,&build,error);
    if(copy_to(dir,"build.stdout",&build.out)<0||copy_to(dir,"build.stderr",&build.err)<0){strcpy(error,"cannot save compiler diagnostics");goto fail;}if(built<0)goto fail;
    if(read_at(dir,"source.c",8U*1024U*1024U,&generated)<0||generated.truncated){strcpy(error,"compiler did not retain expected generated C source");goto fail;}
    generated.data[generated.len]=0;if(!strstr((char *)generated.data,"cobc 3.2.")){strcpy(error,"coverage adapter requires the validated GnuCOBOL 3.2 generated-source format");goto fail;}
    const char *vargs[]={"--version"};if(tool_run(tool,vargs,1,root,&version,error)<0||copy_to(dir,"compiler-version.txt",&version.out)<0)goto fail;
    if(config_capture_environment(&env)<0||copy_to(dir,"environment.bin",&env.environment)<0){strcpy(error,"cannot record compilation environment");goto fail;}
    char source_hash[65],generated_hash[65],target_hash[65],notes_hash[65],compiler_hash[65],path[B_PATH];hash_bytes(input.data,input.len,source_hash);hash_bytes(generated.data,generated.len,generated_hash);
    if(path_join(path,root,"program")<0||hash_file(path,target_hash)<0||path_join(path,root,"source.gcno")<0||hash_file(path,notes_hash)<0||hash_file(tool,compiler_hash)<0){strcpy(error,"compiler did not create binary/coverage notes");goto fail;}
    f=open_memstream(&text,&text_len);if(!f){strcpy(error,"cannot save instrumentation manifest");goto fail;}
    fprintf(f,"format=beryl-coverage-1\nsource_sha256=%s\ngenerated_sha256=%s\ntarget_sha256=%s\nnotes_sha256=%s\ncompiler_sha256=%s\n",source_hash,generated_hash,target_hash,notes_hash,compiler_hash);
    {int bad=ferror(f);if(fclose(f))bad=1;f=NULL;if(bad){strcpy(error,"cannot format instrumentation manifest");goto fail;}}
    marker=text;marker_len=text_len;text=NULL;text_len=0;
    f=open_memstream(&text,&text_len);if(!f){strcpy(error,"cannot save build metadata");goto fail;}fputs("{\"schema\":\"beryl-instrumentation-1\",\"compiler\":",f);json_string(f,tool);fputs(",\"argv\":[",f);for(size_t i=0;i<sizeof args/sizeof *args;i++){if(i)fputc(',',f);json_string(f,args[i]);}fputs("],\"adapter\":\"gnucobol-3.2-gcov-mapped-lines\"}\n",f);
    if(write_stream(dir,"build.json",f,&text,&text_len)<0){f=NULL;strcpy(error,"cannot save build metadata");goto fail;}f=NULL;
    if(write_at(dir,"instrumentation.txt",marker,marker_len)<0){strcpy(error,"cannot save instrumentation completion marker");goto fail;}
    printf("INSTRUMENTED: %s/program\nRun the executable before collecting coverage. Counters accumulate in this build directory.\n",out);rc=0;goto done;
fail:fprintf(stderr,"error: instrument: %s\n",error);
done:if(f)fclose(f);free(text);free(marker);bytes_free(&input);bytes_free(&generated);result_free(&build);result_free(&version);config_free(&env);if(dir>=0)close(dir);return rc;
}
static int verify_artifact(int dir,const char *root,const char *manifest,const char *file,const char *key,Bytes *bytes) {
    char needle[100];snprintf(needle,sizeof needle,"\n%s=",key);const char *hash=strstr(manifest,needle);if(!hash)return -1;hash+=strlen(needle);
    if(strspn(hash,"0123456789abcdef")!=64||hash[64]!='\n')return -1;
    char actual[65],path[B_PATH];
    if(bytes){if(read_at(dir,file,8U*1024U*1024U,bytes)<0||bytes->truncated)return -1;hash_bytes(bytes->data,bytes->len,actual);}
    else if(path_join(path,root,file)<0||hash_file(path,actual)<0)return -1;
    return memcmp(hash,actual,64)?-1:0;
}
int coverage_cli(int argc,char **argv) {
    const char *build_path=NULL,*out=NULL,*gcov="gcov";
    for(int i=2;i<argc;i++){const char *flag=argv[i];if(!strcmp(flag,"--help")){puts("Usage: beryl coverage --build INSTRUMENTED-DIR --out NEW-DIR [--gcov PATH]\n  Verifies source/generated C/binary/notes hashes and reports only mapped COBOL source lines.\n  Requires execution data from an instrumented program; no unverified C-coverage substitution.");return 0;}if(i+1==argc){fprintf(stderr,"error: %s needs a value\n",flag);return 2;}const char *v=argv[++i];if(!strcmp(flag,"--build"))build_path=v;else if(!strcmp(flag,"--out"))out=v;else if(!strcmp(flag,"--gcov"))gcov=v;else{fprintf(stderr,"error: unknown coverage flag %s\n",flag);return 2;}}
    if(!build_path||!out){fputs("error: coverage requires --build and --out\n",stderr);return 2;}
    char error[512]="",root[B_PATH];Bytes manifest={0},source={0},generated={0},data={0};Result result={0};int dir=-1,dest=-1,rc=2;bool *mapped=NULL,*seen=NULL;uint64_t *counts=NULL;char *json=NULL;size_t json_len=0;FILE *f=NULL;
    dir=open_directory(build_path,false);if(dir<0||!realpath(build_path,root)||read_at(dir,"instrumentation.txt",B_ENV_LIMIT,&manifest)<0||manifest.truncated){strcpy(error,"unavailable: invalid instrumentation directory");goto fail;}manifest.data[manifest.len]=0;
    if(strncmp((char *)manifest.data,"format=beryl-coverage-1\n",24)||verify_artifact(dir,root,(char *)manifest.data,"source.cob","source_sha256",&source)<0||verify_artifact(dir,root,(char *)manifest.data,"source.c","generated_sha256",&generated)<0||verify_artifact(dir,root,(char *)manifest.data,"program","target_sha256",NULL)<0||verify_artifact(dir,root,(char *)manifest.data,"source.gcno","notes_sha256",NULL)<0){strcpy(error,"unavailable: source, generated mapping, executable or coverage-notes digest changed");goto fail;}
    if(read_at(dir,"source.gcda",64U*1024U*1024U,&data)<0||data.truncated){strcpy(error,"unavailable: run the instrumented executable first (source.gcda missing/oversized)");goto fail;}
    size_t lines=1;for(size_t i=0;i<source.len;i++)if(source.data[i]=='\n')lines++;
    mapped=calloc(lines+1,sizeof *mapped);seen=calloc(lines+1,sizeof *seen);counts=calloc(lines+1,sizeof *counts);if(!mapped||!seen||!counts){strcpy(error,"out of memory reading coverage mapping");goto fail;}
    generated.data[generated.len]=0;char *state=NULL;
    for(char *line=strtok_r((char *)generated.data,"\n",&state);line;line=strtok_r(NULL,"\n",&state))if(!strncmp(line,"#line ",6)){
        unsigned n;char filename[B_PATH];if(sscanf(line,"#line %u \"%4095[^\"]\"",&n,filename)==2&&!strcmp(filename,"source.cob")){if(!n||n>lines){strcpy(error,"invalid COBOL line mapping");goto fail;}mapped[n]=true;}}
    const char *args[]={"-t","source.gcno"};if(tool_run(gcov,args,2,root,&result,error)<0)goto fail;
    dest=new_directory(out);if(dest<0){strcpy(error,"--out must be new and writable");goto fail;}
    if(copy_to(dest,"gcov.txt",&result.out)<0){strcpy(error,"cannot save raw gcov evidence");goto fail;}
    result.out.data[result.out.len]=0;bool selected=false;state=NULL;
    for(char *line=strtok_r((char *)result.out.data,"\n",&state);line;line=strtok_r(NULL,"\n",&state)) {
        char *first=strchr(line,':');if(!first)continue;char *second=strchr(first+1,':');if(!second)continue;
        if(!strncmp(second+1,"Source:",7)){selected=!strcmp(second+8,"source.cob");continue;}if(!selected)continue;
        char *end;errno=0;unsigned long n=strtoul(first+1,&end,10);if(errno||end!=second||!n||n>lines||!mapped[n])continue;
        *first=0;char *count=line;while(*count==' ')count++;size_t len=strlen(count);while(len&&count[len-1]==' ')count[--len]=0;
        if(!strcmp(count,"-"))continue;
        if(!strcmp(count,"#####")||!strcmp(count,"====="))counts[n]=0;
        else {if(len&&count[len-1]=='*')count[--len]=0;if(parse_u64(count,UINT64_MAX,&counts[n])<0){strcpy(error,"unrecognized gcov count");goto fail;}}
        seen[n]=true;
    }
    size_t total=0,covered=0;for(size_t i=1;i<=lines;i++)if(mapped[i]){if(!seen[i]){snprintf(error,sizeof error,"unavailable: mapped COBOL line %zu has no verified gcov counter",i);goto fail;}total++;if(counts[i])covered++;}
    if(!total){strcpy(error,"unavailable: no compiler-verified COBOL source mappings");goto fail;}
    char hash[65];hash_bytes(source.data,source.len,hash);f=open_memstream(&json,&json_len);if(!f){strcpy(error,"cannot save coverage report");goto fail;}
    fprintf(f,"{\"schema\":\"beryl-coverage-1\",\"adapter\":\"gnucobol-3.2-gcov-mapped-lines\",\"source_sha256\":\"%s\",\"covered_lines\":%zu,\"mapped_lines\":%zu,\"branch_coverage\":null,\"lines\":[",hash,covered,total);bool comma=false;
    for(size_t i=1;i<=lines;i++)if(mapped[i]){fprintf(f,"%s{\"line\":%zu,\"count\":%" PRIu64 "}",comma?",":"",i,counts[i]);comma=true;}
    fputs("]}\n",f);
    if(write_stream(dest,"coverage.json",f,&json,&json_len)<0){f=NULL;strcpy(error,"cannot save coverage.json");goto fail;}f=NULL;printf("COVERAGE: %zu/%zu mapped COBOL source lines observed; branch coverage unavailable\nreport: %s/coverage.json\n",covered,total,out);rc=0;goto done;
fail:fprintf(stderr,"error: coverage: %s\n",error);
done:if(f)fclose(f);free(json);free(mapped);free(seen);free(counts);bytes_free(&manifest);bytes_free(&source);bytes_free(&generated);bytes_free(&data);result_free(&result);if(dest>=0)close(dest);if(dir>=0)close(dir);return rc;
}

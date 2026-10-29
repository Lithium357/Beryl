#include "beryl/compiler_adapter.h"
#include "beryl/copybook.h"
#include "beryl/options.h"
#include "beryl/report.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int find_tool(const char *name,char path[B_PATH],char error[512]) {
    if(strchr(name,'/')){if(realpath(name,path)&&access(path,X_OK)==0)return 0;}
    else {
        const char *env=getenv("PATH");char *copy=strdup(env?env:"/usr/bin:/bin");if(!copy){strcpy(error,"out of memory resolving compiler");return -1;}
        char *state=NULL;for(char *part=strtok_r(copy,":",&state);part;part=strtok_r(NULL,":",&state)){char candidate[B_PATH];if(path_join(candidate,part,name)==0&&realpath(candidate,path)&&access(path,X_OK)==0){free(copy);return 0;}}free(copy);
    }
    snprintf(error,512,"cannot find executable %.300s; install it or provide its explicit path",name);return -1;
}
int tool_run(const char *tool,const char *const args[],size_t count,const char *cwd,Result *r,char error[512]) {
    memset(r,0,sizeof *r);r->exit_code=-1;r->outcome=UNVERIFIED;
    Config c;config_defaults(&c);int fd=-1,rc=-1;
    if(count>B_ARGS||find_tool(tool,c.target,error)<0)goto done;
    for(size_t i=0;i<count;i++)if(config_option(&c,"--arg",args[i])<0){strcpy(error,"compiler argument is too long");goto done;}
    c.timeout_ms=30000;c.cpu_seconds=30;c.max_capture=8U*1024U*1024U;c.max_file=64U*1024U*1024U;
    if(config_capture_environment(&c)<0||(fd=open_directory(cwd,false))<0||run_child(&c,r,fd)<0){snprintf(error,512,"compiler/tool execution failed: %s",strerror(errno));goto done;}
    if(r->exit_code||r->signal_number||r->outcome!=UNVERIFIED||r->out.truncated||r->err.truncated){snprintf(error,512,"%.200s failed: exit=%d signal=%d classification=%s (inspect saved stderr)",tool,r->exit_code,r->signal_number,outcome_name(r->outcome));goto done;}
    rc=0;
done:if(fd>=0)close(fd);config_free(&c);return rc;
}
int preprocess_cli(int argc,char **argv) {
    const char *source_path=NULL,*out=NULL,*format="free",*compiler="cobc",*includes[16];size_t include_count=0;
    for(int i=2;i<argc;i++){const char *flag=argv[i];if(!strcmp(flag,"--help")){puts("Usage: beryl preprocess --copybook FILE --out NEW-DIR [--format free|fixed] [--include DIR] [--cobc PATH]\n  Uses GnuCOBOL -E with explicit argv to resolve COPY. The final layout must satisfy Beryl's strict subset.");return 0;}
        if(i+1==argc){fprintf(stderr,"error: %s needs a value\n",flag);return 2;}const char *v=argv[++i];
        if(!strcmp(flag,"--copybook"))source_path=v;else if(!strcmp(flag,"--out"))out=v;else if(!strcmp(flag,"--format"))format=v;else if(!strcmp(flag,"--cobc"))compiler=v;
        else if(!strcmp(flag,"--include")&&include_count<16)includes[include_count++]=v;else{fprintf(stderr,"error: invalid preprocessing flag %s\n",flag);return 2;}}
    if(!source_path||!out||(strcmp(format,"free")&&strcmp(format,"fixed"))){fputs("error: preprocess needs --copybook, --out and --format free|fixed\n",stderr);return 2;}
    char error[512]="",tool[B_PATH],source_absolute[B_PATH],root[B_PATH];Bytes source={0};Result result={0},version={0};int rc=2,dir=-1;char *resolved=NULL,*metadata=NULL;size_t metadata_len=0;FILE *f=NULL;
    if(find_tool(compiler,tool,error)<0)goto fail;
    if(!realpath(source_path,source_absolute)||read_bytes(source_absolute,B_ENV_LIMIT,&source)<0||source.truncated){strcpy(error,"cannot read bounded copybook source");goto fail;}
    dir=new_directory(out);if(dir<0||!realpath(out,root)){snprintf(error,sizeof error,"--out must be new and writable: %s",strerror(errno));goto fail;}
    if(copy_to(dir,"original.cpy",&source)<0){strcpy(error,"cannot archive source");goto fail;}
    char source_parent[B_PATH];strcpy(source_parent,source_absolute);char *slash=strrchr(source_parent,'/');if(slash==source_parent)source_parent[1]=0;else *slash=0;
    char include_paths[16][B_PATH];const char *args[B_ARGS];size_t n=0;args[n++]="-E";args[n++]=!strcmp(format,"fixed")?"-fixed":"-free";args[n++]="-I";args[n++]=source_parent;
    for(size_t i=0;i<include_count;i++){if(!realpath(includes[i],include_paths[i])){strcpy(error,"cannot resolve --include directory");goto fail;}args[n++]="-I";args[n++]=include_paths[i];}args[n++]=source_absolute;
    int executed=tool_run(tool,args,n,root,&result,error);
    if(copy_to(dir,"compiler.stdout",&result.out)<0||copy_to(dir,"compiler.stderr",&result.err)<0){strcpy(error,"cannot save compiler diagnostics");goto fail;}
    if(executed<0)goto fail;
    if(result.out.len>B_ENV_LIMIT){strcpy(error,"expanded copybook exceeds 1 MiB");goto fail;}
    resolved=malloc(result.out.len+1);if(!resolved){strcpy(error,"out of memory resolving source");goto fail;}
    size_t used=0;
    for(size_t at=0;at<result.out.len;){size_t end=at;while(end<result.out.len&&result.out.data[end]!='\n')end++;
        if(end-at<6||memcmp(result.out.data+at,"#line ",6)){memcpy(resolved+used,result.out.data+at,end-at);used+=end-at;resolved[used++]='\n';}at=end<result.out.len?end+1:end;}
    Layout layout;if(copybook_parse(resolved,used,&layout,error)<0)goto fail;
    if(write_at(dir,"resolved.cpy",resolved,used)<0){strcpy(error,"cannot save resolved copybook");goto fail;}
    const char *version_args[]={"--version"};if(tool_run(tool,version_args,1,root,&version,error)<0||copy_to(dir,"compiler-version.txt",&version.out)<0)goto fail;
    char a[65],b[65],c[65];hash_bytes(source.data,source.len,a);hash_bytes(resolved,used,b);if(hash_file(tool,c)<0){strcpy(error,"cannot hash compiler");goto fail;}
    f=open_memstream(&metadata,&metadata_len);if(!f){strcpy(error,"cannot save preprocessing metadata");goto fail;}
    fprintf(f,"{\"schema\":\"beryl-preprocess-1\",\"source_sha256\":\"%s\",\"resolved_sha256\":\"%s\",\"compiler_sha256\":\"%s\",\"format\":\"free\",\"input_format\":",a,b,c);json_string(f,format);fputs(",\"compiler\":",f);json_string(f,tool);fputs(",\"argv\":[",f);for(size_t i=0;i<n;i++){if(i)fputc(',',f);json_string(f,args[i]);}fputs("]}\n",f);
    {int bad=ferror(f);if(fclose(f))bad=1;f=NULL;if(bad||write_at(dir,"preprocess.json",metadata,metadata_len)<0){strcpy(error,"cannot save preprocessing manifest");goto fail;}}
    printf("PREPROCESSED: %zu fields, %zu-byte record\ncopybook: %s/resolved.cpy (free format)\n",layout.count,layout.size,out);rc=0;goto done;
fail:fprintf(stderr,"error: preprocess: %s\n",error);
done:if(f)fclose(f);free(metadata);free(resolved);bytes_free(&source);result_free(&result);result_free(&version);if(dir>=0)close(dir);return rc;
}

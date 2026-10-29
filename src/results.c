#include "beryl/beryl.h"
#include "beryl/report.h"
#include "beryl/fixtures.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
extern char **environ;
void config_defaults(Config *c) {
    memset(c,0,sizeof *c);strcpy(c->input_name,"INPUT.DAT");strcpy(c->output_name,"OUTPUT.DAT");strcpy(c->results,".beryl/results");strcpy(c->compiler,"unknown");
    c->timeout_ms=2000;c->cpu_seconds=2;c->max_input=1024U*1024U;c->max_capture=1024U*1024U;c->max_file=16U*1024U*1024U;
}
void config_free(Config *c) { for(size_t i=0;i<c->argc;i++)free(c->args[i]);bytes_free(&c->environment); }
void result_free(Result *r) {
    bytes_free(&r->out);bytes_free(&r->err);bytes_free(&r->output);
    for(size_t i=0;i<r->extra_count;i++)bytes_free(&r->extra[i].bytes);
    if(r->reference){result_free(r->reference);free(r->reference);r->reference=NULL;}
}
int config_capture_environment(Config *c) {
    size_t len=0;for(size_t i=0;environ[i];i++){size_t n=strlen(environ[i])+1;if(n>B_ENV_LIMIT-len){errno=E2BIG;return -1;}len+=n;}
    c->environment.data=malloc(len+1);if(!c->environment.data)return -1;
    for(size_t i=0;environ[i];i++){size_t n=strlen(environ[i])+1;memcpy(c->environment.data+c->environment.len,environ[i],n);c->environment.len+=n;}return 0;
}
/* Text values are hex encoded, allowing newlines and arbitrary path/argument bytes without injection. */
static void hex_value(FILE *f,const char *key,const void *data,size_t n) {
    const unsigned char *p=data;fprintf(f,"%s=",key);for(size_t i=0;i<n;i++)fprintf(f,"%02x",(unsigned)p[i]);fputc('\n',f);
}
static void text_value(FILE *f,const char *key,const char *s) { hex_value(f,key,s,strlen(s)); }
int save_result(const Config *c,const Result *r,int archive) {
    char *buf=NULL;size_t len=0;FILE *f=open_memstream(&buf,&len);if(!f)return -1;
    fprintf(f,"format=%s\nversion=%s\nclassification=%s\n",c->input_count||c->output_count?"beryl-2":"beryl-1",BERYL_VERSION,outcome_name(r->outcome));
    text_value(f,"target",c->target);text_value(f,"input_name",c->input_name);text_value(f,"output_name",c->output_name);text_value(f,"compiler",c->compiler);text_value(f,"diagnostic",r->diagnostic);
    text_value(f,"reference",c->reference);
    fprintf(f,"reference_sha256=%s\ncomparison=%u\nnormalize_crlf=%d\ndifferences=%u\n",r->reference?r->reference->target_hash:"",c->comparison,c->normalize_crlf,r->differences);
    fprintf(f,"target_sha256=%s\ninput_sha256=%s\nexpected_sha256=%s\noutput_sha256=%s\n",r->target_hash,r->input_hash,r->expected_hash,r->output_hash);
    char envhash[65];hash_bytes(c->environment.data,c->environment.len,envhash);fprintf(f,"environment_sha256=%s\n",envhash);
    fprintf(f,"has_expected=%d\nexpected_exit=%d\ntimeout_ms=%u\ncpu_seconds=%u\nmax_input=%zu\nmax_capture=%zu\nmax_file=%zu\n",c->has_expected,c->expected_exit,c->timeout_ms,c->cpu_seconds,c->max_input,c->max_capture,c->max_file);
    fprintf(f,"exit_code=%d\nsignal=%d\nexec_errno=%d\nduration_ms=%" PRIu64 "\nstdout_truncated=%d\nstderr_truncated=%d\noutput_truncated=%d\nmissing_output=%d\n",r->exit_code,r->signal_number,r->exec_errno,r->duration_ms,r->out.truncated,r->err.truncated,r->output.truncated,r->missing_output);
    if(c->seeded)fprintf(f,"seed=%" PRIu64 "\ncase_index=%" PRIu64 "\n",c->seed,c->case_index);else fputs("seed=none\n",f);
    text_value(f,"mutation",c->mutation);fprintf(f,"layout_sha256=%s\n",c->layout_hash);
    fixtures_manifest(f,c,r);
    text_value(f,"argv0",c->target);fprintf(f,"argc=%zu\n",c->argc);
    for(size_t i=0;i<c->argc;i++){char key[32];snprintf(key,sizeof key,"arg%zu",i);text_value(f,key,c->args[i]);}
    char work_path[B_PATH];
    if(path_join(work_path,r->directory,"work")<0){fclose(f);free(buf);return -1;}
    text_value(f,"work_directory",work_path);
    fprintf(f,"output_hash_scope=%s\n",r->output.truncated?"captured-prefix":"complete");
    int bad=ferror(f);if(fclose(f)!=0)bad=1;
    if(bad){free(buf);return -1;}
    if(copy_to(archive,"stdout.bin",&r->out)<0 || copy_to(archive,"stderr.bin",&r->err)<0 ||
       (!r->missing_output && r->output.data && copy_to(archive,"output.bin",&r->output)<0)) {
        free(buf);return -1;
    }
    if(fixtures_save(c,r,archive)<0||save_reports(c,r,archive)<0){free(buf);return -1;}
    /* A manifest is the completion marker: never publish PASS before evidence is saved. */
    int rc=write_at(archive,"manifest.txt",buf,len);free(buf);return rc;
}
int execute_case(Config *c,Result *r) {
    memset(r,0,sizeof *r);r->exit_code=-1;r->outcome=UNVERIFIED;
    int archive=-1,work=-1,rc=-1;Bytes input={0},expected={0},additional[B_EXTRA_FILES]={{0}};char file_error[512];
    const char *stage="validate declared files";
    if(fixtures_validate(c,file_error)<0){stage=file_error;errno=EINVAL;goto fail;}r->extra_count=c->output_count;
    stage="read --input";
    if(read_bytes(c->input,c->max_input,&input)<0)goto fail;
    if(input.truncated){errno=EFBIG;goto fail;}
    stage="read --expected";
    if(c->has_expected && (read_bytes(c->expected,c->max_capture,&expected)<0 || expected.truncated)){if(expected.truncated)errno=EFBIG;goto fail;}
    hash_bytes(input.data,input.len,r->input_hash);
    if(c->has_expected)hash_bytes(expected.data,expected.len,r->expected_hash);
    stage="hash --exec (provide a regular executable)";if(hash_file(c->target,r->target_hash)<0)goto fail;
    stage="capture environment";if(!c->environment.data && config_capture_environment(c)<0)goto fail;
    stage="create private --results workspace (avoid symlinks)";if(workspace_create(c,r,&archive,&work)<0)goto fail;
    stage="archive fixtures";
    if(copy_to(archive,"input.bin",&input)<0 || copy_to(archive,"environment.bin",&c->environment)<0 || (c->has_expected && copy_to(archive,"expected.bin",&expected)<0))goto fail;
    stage="copy --input-name";if(copy_to(work,c->input_name,&input)<0)goto fail;
    /* Create declared output parents before execution; do not pre-create the output file. */
    char leaf[B_PATH];int parent=open_parent(work,c->output_name,true,leaf);if(parent<0)goto fail;close(parent);
    if(fixtures_stage(c,r,archive,work,input.len,additional,file_error)<0){stage=file_error;goto fail;}
    if(*c->reference) {
        stage="execute reference";
        Config reference=*c;strcpy(reference.target,c->reference);reference.reference[0]=0;reference.has_expected=false;
        if(path_join(reference.input,r->directory,"input.bin")<0 || path_join(reference.results,r->directory,"reference")<0)goto fail;
        for(size_t i=0;i<c->input_count;i++){char name[64];snprintf(name,sizeof name,"input-%02zu.bin",i);if(path_join(reference.inputs[i].path,r->directory,name)<0)goto fail;}
        for(size_t i=0;i<c->output_count;i++)reference.outputs[i].oracle=false;
        r->reference=calloc(1,sizeof *r->reference);if(!r->reference)goto fail;
        (void)execute_case(&reference,r->reference);
    }
    stage="execute child";if(run_child(c,r,work)<0)goto fail;
    stage="capture --output-name (must be a regular file without symlinks)";
    if(read_at(work,c->output_name,c->max_capture,&r->output)<0) {
        if(errno==ENOENT)r->missing_output=true;
        else goto fail;
    } else hash_bytes(r->output.data,r->output.len,r->output_hash);
    if(fixtures_capture(c,r,work,file_error)<0){stage=file_error;goto fail;}
    if(r->reference)compare_reference(c,r,r->reference);else {compare_result(c,r,&expected);fixtures_golden(c,r,additional);}
    stage="save result evidence";if(save_result(c,r,archive)<0)goto fail;
    rc=0;goto done;
fail:
    r->outcome=HARNESS_ERROR;snprintf(r->diagnostic,sizeof r->diagnostic,"%.400s: %.100s",stage,strerror(errno));
    if(archive>=0)(void)save_result(c,r,archive);
done:
    if(work>=0)close(work);
    if(archive>=0)close(archive);
    bytes_free(&input);bytes_free(&expected);for(size_t i=0;i<B_EXTRA_FILES;i++)bytes_free(&additional[i]);return rc;
}
static int nibble(char c) { if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1; }
static int decode(const char *value,char *out,size_t cap) {
    size_t n=strlen(value);if(n%2 || n/2>=cap)return -1;
    for(size_t i=0;i<n;i+=2){int a=nibble(value[i]),b=nibble(value[i+1]);if(a<0||b<0||!(a||b))return -1;out[i/2]=(char)(a*16+b);}out[n/2]=0;return 0;
}
static int number(const char *s,unsigned long long *v) {
    if(!*s)return -1;
    for(const char *p=s;*p;p++)if(*p<'0'||*p>'9')return -1;
    char *end;errno=0;*v=strtoull(s,&end,10);return errno||*end?-1:0;
}
static int verify_at(int dir,const char *name,size_t max,const char *hash,Bytes *b) {
    char actual[65];if(strlen(hash)!=64 || read_at(dir,name,max,b)<0 || b->truncated)return -1;
    hash_bytes(b->data,b->len,actual);return strcmp(actual,hash)?-1:0;
}
int replay_load(const char *directory,Config *c,char saved_target_hash[65]) {
    int dir=open_directory(directory,false);if(dir<0)return -1;
    Bytes manifest={0},input={0},expected={0};int rc=-1;
    char ih[65]="",eh[65]="",envh[65]="";unsigned seen=0,extra_counts=0;bool file_set_format=false;size_t declared_args=0,seen_args=0;
    if(read_at(dir,"manifest.txt",B_ENV_LIMIT,&manifest)<0 || manifest.truncated)goto done;
    manifest.data[manifest.len]=0;
    if(memchr(manifest.data,0,manifest.len))goto done;
    char *state=NULL;
    for(char *line=strtok_r((char *)manifest.data,"\n",&state);line;line=strtok_r(NULL,"\n",&state)) {
        char *v=strchr(line,'=');if(!v)goto done;*v++=0;
        if(!strcmp(line,"extra_inputs"))extra_counts|=1;
        if(!strcmp(line,"extra_outputs"))extra_counts|=2;
        int fixture=fixtures_parse(c,line,v);if(fixture<0)goto done;if(fixture>0)continue;
        if(!strcmp(line,"format")){if(strcmp(v,"beryl-1")&&strcmp(v,"beryl-2"))goto done;file_set_format=!strcmp(v,"beryl-2");seen|=1;}
        else if(!strcmp(line,"target")){if(decode(v,c->target,sizeof c->target)<0)goto done;seen|=2;}
        else if(!strcmp(line,"input_name")){if(decode(v,c->input_name,sizeof c->input_name)<0)goto done;seen|=4;}
        else if(!strcmp(line,"output_name")){if(decode(v,c->output_name,sizeof c->output_name)<0)goto done;seen|=8;}
        else if(!strcmp(line,"compiler")){if(decode(v,c->compiler,sizeof c->compiler)<0)goto done;}
        else if(!strcmp(line,"reference")){if(decode(v,c->reference,sizeof c->reference)<0)goto done;}
        else if(!strcmp(line,"reference_sha256")){if(strlen(v)>64)goto done;strcpy(c->replay_reference_hash,v);}
        else if(!strcmp(line,"seed")&&strcmp(v,"none")) { char *end;errno=0;if(!*v||*v=='-')goto done;c->seed=strtoull(v,&end,10);if(errno||*end)goto done;c->seeded=true; }
        else if(!strcmp(line,"case_index")) { unsigned long long n;if(number(v,&n)<0)goto done;c->case_index=(uint64_t)n; }
        else if(!strcmp(line,"mutation")){if(decode(v,c->mutation,sizeof c->mutation)<0)goto done;}
        else if(!strcmp(line,"layout_sha256")){if(strlen(v)>64)goto done;strcpy(c->layout_hash,v);}
        else if(!strcmp(line,"comparison") || !strcmp(line,"normalize_crlf")) {
            unsigned long long n;if(number(v,&n)<0 || n>(!strcmp(line,"comparison")?2U:1U))goto done;
            if(!strcmp(line,"comparison"))c->comparison=(unsigned)n;else c->normalize_crlf=n!=0;
        }
        else if(!strcmp(line,"target_sha256") || !strcmp(line,"input_sha256") || !strcmp(line,"expected_sha256") || !strcmp(line,"environment_sha256")) {
            if(strlen(v)>64)goto done;
            char *dest=!strcmp(line,"target_sha256")?saved_target_hash:!strcmp(line,"input_sha256")?ih:!strcmp(line,"expected_sha256")?eh:envh;strcpy(dest,v);
        }
        else if(!strcmp(line,"argc")){unsigned long long n;if(number(v,&n)<0||n>B_ARGS)goto done;declared_args=(size_t)n;seen|=16;}
        else if(!strncmp(line,"arg",3) && strcmp(line,"argv0")) {
            unsigned long long n;if(number(line+3,&n)<0 || n!=seen_args || n>=B_ARGS)goto done;
            char decoded[B_PATH];if(decode(v,decoded,sizeof decoded)<0)goto done;
            c->args[c->argc]=strdup(decoded);if(!c->args[c->argc])goto done;c->argc++;seen_args++;
        }
        else {
            const char *keys[]={"has_expected","expected_exit","timeout_ms","cpu_seconds","max_input","max_capture","max_file"};
            for(unsigned i=0;i<7;i++)if(!strcmp(line,keys[i])) {
                unsigned long long n;if(number(v,&n)<0 || n>64U*1024U*1024U)goto done;
                if(i==0){if(n>1)goto done;c->has_expected=n!=0;}
                if(i==1){if(n>255)goto done;c->expected_exit=(int)n;}
                if(i==2)c->timeout_ms=(unsigned)n;
                if(i==3)c->cpu_seconds=(unsigned)n;
                if(i==4)c->max_input=(size_t)n;
                if(i==5)c->max_capture=(size_t)n;
                if(i==6)c->max_file=(size_t)n;
                seen|=32U<<i;
            }
        }
    }
    if(seen!=4095U || declared_args!=seen_args || !safe_name(c->input_name) || !safe_name(c->output_name) || !strcmp(c->input_name,c->output_name) || !c->timeout_ms || c->timeout_ms>600000U || !c->cpu_seconds || c->cpu_seconds>3600U || !c->max_input || !c->max_capture || !c->max_file || strlen(saved_target_hash)!=64)goto done;
    if(verify_at(dir,"input.bin",c->max_input,ih,&input)<0 || (c->has_expected && verify_at(dir,"expected.bin",c->max_capture,eh,&expected)<0) || verify_at(dir,"environment.bin",B_ENV_LIMIT,envh,&c->environment)<0)goto done;
    if(c->environment.len && c->environment.data[c->environment.len-1])goto done;
    if((file_set_format&&extra_counts!=3)||(!file_set_format&&(c->input_count||c->output_count)))goto done;
    if(*c->reference && (strlen(c->replay_reference_hash)!=64 || c->has_expected))goto done;
    char resolved[B_PATH];if(!realpath(directory,resolved) || path_join(c->input,resolved,"input.bin")<0 || path_join(c->expected,resolved,"expected.bin")<0)goto done;
    if(fixtures_replay(c,dir,resolved,input.len)<0)goto done;
    rc=0;
done:
    bytes_free(&manifest);bytes_free(&input);bytes_free(&expected);close(dir);if(rc<0)errno=EINVAL;return rc;
}

#include "beryl/campaign.h"
#include "beryl/options.h"
#include "beryl/record_codec.h"
#include "beryl/report.h"
#include <errno.h>
#include <dirent.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef struct {
    Config *config;Result *baseline;Bytes current;
    char root[B_PATH],accepted[B_PATH],error[512];
    int directory;uint64_t deadline,max_tries,tries;bool signature;
    const char *stop;
} Reduction;
static uint64_t complexity(const unsigned char *data,size_t len) {
    uint64_t score=0;
    for(size_t i=0;i<len;i++)if(data[i]&&data[i]!=' '&&data[i]!='0')score+=data[i];
    return score;
}
static bool room(Reduction *m) {
    if(m->tries>=m->max_tries){m->stop="try-limit";return false;}
    uint64_t now=monotonic_ms(),worst=(uint64_t)m->config->timeout_ms*(*m->config->reference?2U:1U);
    if(now>=m->deadline||worst>m->deadline-now){m->stop="time-budget";return false;}
    return true;
}
static bool probe(Reduction *m,const unsigned char *data,size_t len,const char *operation) {
    if(len==m->current.len&&!memcmp(data,m->current.data,len))return false;
    if(len>m->current.len||(len==m->current.len&&complexity(data,len)>=complexity(m->current.data,m->current.len)))return false;
    if(!room(m))return false;
    char name[64];snprintf(name,sizeof name,"trial-%06" PRIu64 ".bin",m->tries);
    if(write_at(m->directory,name,data,len)<0||path_join(m->config->input,m->root,name)<0){strcpy(m->error,"cannot save minimization trial");return false;}
    snprintf(m->config->mutation,sizeof m->config->mutation,"minimize:%.240s",operation);
    Result result;(void)execute_case(m->config,&result);m->tries++;
    bool matches=failure_equivalent(m->baseline,&result,m->signature);
    if(matches&&room(m)) {
        Result confirmation;(void)execute_case(m->config,&confirmation);m->tries++;
        char a[65],b[65];failure_signature(&result,a);failure_signature(&confirmation,b);
        matches=failure_equivalent(m->baseline,&confirmation,m->signature)&&!strcmp(a,b);
        if(matches){memcpy(m->current.data,data,len);m->current.len=len;strcpy(m->accepted,confirmation.directory);}
        result_free(&confirmation);
    } else matches=false;
    result_free(&result);return matches;
}
static int saved_result(const char *directory,size_t cap,Result *r,bool nested) {
    memset(r,0,sizeof *r);r->exit_code=-1;int dir=open_directory(directory,false);if(dir<0)return -1;
    Bytes b={0};int rc=-1;bool classified=false,reference=false;
    if(read_at(dir,"manifest.txt",B_ENV_LIMIT,&b)<0||b.truncated)goto done;
    b.data[b.len]=0;char *state=NULL;
    for(char *line=strtok_r((char *)b.data,"\n",&state);line;line=strtok_r(NULL,"\n",&state)) {
        char *v=strchr(line,'=');if(!v)goto done;*v++=0;
        if(!strcmp(line,"classification")){for(int i=PASS;i<=REGRESSION_CANDIDATE;i++)if(!strcmp(v,outcome_name((Outcome)i))){r->outcome=(Outcome)i;classified=true;}if(!classified)goto done;}
        else if(!strcmp(line,"diagnostic")) {
            size_t n=strlen(v);if(n%2||n/2>=sizeof r->diagnostic)goto done;
            for(size_t i=0;i<n;i+=2){char pair[3]={v[i],v[i+1],0};char *end;unsigned long value=strtoul(pair,&end,16);if(*end||!value||value>255)goto done;r->diagnostic[i/2]=(char)value;}
        } else if(!strcmp(line,"reference"))reference=*v!=0;
        else if(!strcmp(line,"output_sha256")){if(strlen(v)>64)goto done;strcpy(r->output_hash,v);}
        else if(!strcmp(line,"exit_code")){char *end;errno=0;long n=strtol(v,&end,10);if(errno||*end||n< -1||n>255)goto done;r->exit_code=(int)n;}
        else {
            const char *keys[]={"signal","differences","missing_output","output_truncated","stdout_truncated","stderr_truncated"};
            for(unsigned i=0;i<6;i++)if(!strcmp(line,keys[i])){uint64_t n;if(parse_u64(v,i<2?255U:1U,&n)<0)goto done;
                if(i==0)r->signal_number=(int)n;else if(i==1)r->differences=(unsigned)n;else if(i==2)r->missing_output=n!=0;else if(i==3)r->output.truncated=n!=0;else if(i==4)r->out.truncated=n!=0;else r->err.truncated=n!=0;}
        }
    }
    if(!classified)goto done;
    bool stdout_truncated=r->out.truncated,stderr_truncated=r->err.truncated,output_truncated=r->output.truncated;
    if(read_at(dir,"stdout.bin",cap,&r->out)<0||r->out.truncated||read_at(dir,"stderr.bin",cap,&r->err)<0||r->err.truncated)goto done;
    r->out.truncated=stdout_truncated;r->err.truncated=stderr_truncated;
    if(!r->missing_output) {
        char hash[65];if(read_at(dir,"output.bin",cap,&r->output)<0||r->output.truncated)goto done;
        hash_bytes(r->output.data,r->output.len,hash);if(strcmp(hash,r->output_hash))goto done;r->output.truncated=output_truncated;
    }
    if(reference) {
        if(nested)goto done;
        char parent[B_PATH],child[B_PATH]="";if(path_join(parent,directory,"reference")<0)goto done;
        int parent_fd=open_directory(parent,false);if(parent_fd<0)goto done;DIR *listing=fdopendir(parent_fd);if(!listing){close(parent_fd);goto done;}
        struct dirent *entry;unsigned count=0;
        while((entry=readdir(listing)))if(!strncmp(entry->d_name,"case-",5)){if(path_join(child,parent,entry->d_name)<0){closedir(listing);goto done;}count++;}
        closedir(listing);if(count!=1)goto done;r->reference=calloc(1,sizeof *r->reference);if(!r->reference||saved_result(child,cap,r->reference,true)<0)goto done;
    }
    rc=0;
done:bytes_free(&b);close(dir);if(rc<0)result_free(r);return rc;
}
static void smallest(const Field *field,bool negative,char value[B_DIGITS+4]) {
    size_t n=0;if(negative)value[n++]='-';value[n++]='0';
    if(field->scale){value[n++]='.';for(unsigned i=0;i<field->scale;i++)value[n++]='0';value[n-1]='1';}else value[n-1]='1';value[n]=0;
}
int minimize_cli(int argc,char **argv) {
    if(argc==3&&!strcmp(argv[2],"--help")){puts("Usage: beryl minimize RESULT-DIR --out NEW-DIR [--copybook FILE] [--byte-fallback]\n  --predicate classification|signature (classification), --max-tries N (200), --budget-ms N (30000)\n  --exec PATH / --reference PATH deliberately override archived executables. Original and reduced bytes are retained.");return 0;}
    if(argc<3){fputs("error: minimize needs a result directory; see beryl minimize --help\n",stderr);return 2;}
    Config c;config_defaults(&c);char expected_target[65]="",error[512]="";Outcome original=UNVERIFIED;int rc=2,dir=-1;bool target_override=false,ref_override=false,signature=false,byte_fallback=false;
    const char *out=NULL,*copybook=NULL,*format="free";uint64_t tries=200,budget=30000;Bytes input={0},source={0};Result baseline={0},archived={0};bool have_baseline=false;
    char *json=NULL;size_t json_len=0;FILE *report=NULL;
    if(replay_load(argv[2],&c,expected_target)<0||saved_result(argv[2],c.max_capture,&archived,false)<0){strcpy(error,"invalid archived case or fixture digest");goto fail;}original=archived.outcome;
    if(original==PASS||original==UNVERIFIED||original==HARNESS_ERROR||original==EXEC_ERROR){strcpy(error,"minimize requires an archived assertion, regression candidate, crash or timeout");goto fail;}
    for(int i=3;i<argc;i++) {
        const char *flag=argv[i];if(!strcmp(flag,"--byte-fallback")){byte_fallback=true;continue;}
        if(i+1==argc){snprintf(error,sizeof error,"%s needs a value",flag);goto fail;}const char *v=argv[++i];
        if(!strcmp(flag,"--out"))out=v;else if(!strcmp(flag,"--copybook"))copybook=v;
        else if(!strcmp(flag,"--format"))format=v;
        else if(!strcmp(flag,"--predicate")){if(!strcmp(v,"signature"))signature=true;else if(strcmp(v,"classification")){strcpy(error,"--predicate requires classification or signature");goto fail;}}
        else if(!strcmp(flag,"--max-tries")){if(parse_u64(v,10000,&tries)<0||!tries){strcpy(error,"--max-tries must be 1..10000");goto fail;}}
        else if(!strcmp(flag,"--budget-ms")){if(parse_u64(v,3600000,&budget)<0||!budget){strcpy(error,"--budget-ms must be 1..3600000");goto fail;}}
        else if(!strcmp(flag,"--exec")){if(config_option(&c,flag,v)<0)goto bad_option;target_override=true;}
        else if(!strcmp(flag,"--reference")){if(config_option(&c,flag,v)<0)goto bad_option;ref_override=true;}
        else {bad_option:snprintf(error,sizeof error,"unsupported minimize option: %s",flag);goto fail;}
    }
    if(!out){strcpy(error,"--out NEW-DIR is required");goto fail;}if(config_resolve(&c,error)<0)goto fail;
    char actual[65];if(!target_override&&(hash_file(c.target,actual)<0||strcmp(actual,expected_target))){strcpy(error,"target digest changed; use --exec deliberately");goto fail;}
    if(*c.reference&&!ref_override&&(hash_file(c.reference,actual)<0||strcmp(actual,c.replay_reference_hash))){strcpy(error,"reference digest changed; use --reference deliberately");goto fail;}
    if(read_bytes(c.input,c.max_input,&input)<0||input.truncated){strcpy(error,"cannot read original input");goto fail;}
    Layout layout;
    if(copybook){if(read_bytes(copybook,B_ENV_LIMIT,&source)<0||source.truncated){strcpy(error,"cannot read --copybook (regular source up to 1 MiB required)");goto fail;}
        if(copybook_parse_format((char *)source.data,source.len,format,&layout,error)<0)goto fail;
        if(record_validate(&layout,input.data,input.len,error)<0)goto fail;
        hash_bytes(source.data,source.len,actual);if(*c.layout_hash&&strcmp(actual,c.layout_hash)){strcpy(error,"--copybook digest differs from archived layout");goto fail;}}
    dir=new_directory(out);if(dir<0){snprintf(error,sizeof error,"--out must be new and writable: %s",strerror(errno));goto fail;}
    Reduction m={0};m.config=&c;m.directory=dir;m.max_tries=tries;m.signature=signature;m.stop="search-complete";
    if(!realpath(out,m.root)||path_join(c.results,m.root,"trials")<0){strcpy(error,"cannot resolve output paths");goto fail;}
    if(copy_to(dir,"original.bin",&input)<0||(source.data&&copy_to(dir,"copybook.cpy",&source)<0)){strcpy(error,"cannot archive original");goto fail;}
    uint64_t start=monotonic_ms();m.deadline=start+budget;
    if((uint64_t)c.timeout_ms*(*c.reference?4U:2U)>budget){strcpy(error,"--budget-ms must allow two full baseline timeout windows");goto fail;}
    (void)execute_case(&c,&baseline);have_baseline=true;Result again;(void)execute_case(&c,&again);
    char x[65],y[65];failure_signature(&baseline,x);failure_signature(&again,y);
    if(!failure_equivalent(&archived,&baseline,true)||!failure_equivalent(&baseline,&again,true)||strcmp(x,y)){result_free(&again);strcpy(error,"original failure is not reproducible; reduction refused");goto fail;}
    strcpy(m.accepted,again.directory);result_free(&again);m.baseline=&baseline;m.current.data=malloc(input.len+1);if(!m.current.data){strcpy(error,"out of memory reducing input");goto fail;}
    memcpy(m.current.data,input.data,input.len);m.current.len=input.len;unsigned char *scratch=malloc(input.len+1);if(!scratch){bytes_free(&m.current);strcpy(error,"out of memory reducing input");goto fail;}
    if(copybook)for(size_t i=0;i<layout.count&&room(&m);i++) {
        const Field *field=&layout.fields[i];
        for(unsigned kind=0;kind<(field->storage==B_TEXT?1U:5U)&&room(&m);kind++) {
            char value[B_DIGITS+4]="0",operation[256];
            if(field->storage==B_TEXT)value[0]=0;
            else if(kind==1)strcpy(value,"1");else if(kind==2){if(!field->signed_value)continue;strcpy(value,"-1");}
            else if(kind>=3){if(kind==4&&!field->signed_value)continue;smallest(field,kind==4,value);}
            memcpy(scratch,m.current.data,m.current.len);if(field_encode(field,value,scratch+field->offset,error)<0)continue;
            snprintf(operation,sizeof operation,"field:%.240s",field->name);(void)probe(&m,scratch,m.current.len,operation);
        }
    }
    if(!copybook||byte_fallback) {
        size_t chunk=m.current.len/2;if(!chunk&&m.current.len)chunk=1;
        while(chunk&&room(&m)) {
            bool changed=false;
            for(size_t at=0;at<m.current.len&&room(&m);) {
                size_t remove=chunk;if(remove>m.current.len-at)remove=m.current.len-at;
                memcpy(scratch,m.current.data,at);memcpy(scratch+at,m.current.data+at+remove,m.current.len-at-remove);
                char operation[128];snprintf(operation,sizeof operation,"delete:%zu:%zu",at,remove);
                if(probe(&m,scratch,m.current.len-remove,operation))changed=true;else at+=remove;
            }
            if(!changed)chunk/=2;
        }
        for(size_t at=0;at<m.current.len&&room(&m);at++)if(m.current.data[at]){memcpy(scratch,m.current.data,m.current.len);scratch[at]=0;(void)probe(&m,scratch,m.current.len,"zero-byte");}
    }
    free(scratch);
    if(*m.error){bytes_free(&m.current);strcpy(error,m.error);goto fail;}
    if(copy_to(dir,"minimized.bin",&m.current)<0){bytes_free(&m.current);strcpy(error,"cannot save reduced bytes");goto fail;}
    char original_hash[65],reduced_hash[65];hash_bytes(input.data,input.len,original_hash);hash_bytes(m.current.data,m.current.len,reduced_hash);
    report=open_memstream(&json,&json_len);if(!report){bytes_free(&m.current);strcpy(error,"cannot save reduction report");goto fail;}
    fprintf(report,"{\"schema\":\"beryl-minimization-1\",\"classification\":\"%s\",\"predicate\":\"%s\",\"original_bytes\":%zu,\"minimized_bytes\":%zu,\"original_sha256\":\"%s\",\"minimized_sha256\":\"%s\",\"trial_executions\":%" PRIu64 ",\"stop_reason\":",outcome_name(original),signature?"signature":"classification",input.len,m.current.len,original_hash,reduced_hash,m.tries);json_string(report,m.stop);fputs(",\"confirmed_result\":",report);json_string(report,m.accepted);fputs("}\n",report);
    {int bad=ferror(report);if(fclose(report))bad=1;report=NULL;if(bad||write_at(dir,"minimization.json",json,json_len)<0){bytes_free(&m.current);strcpy(error,"cannot save minimization.json");goto fail;}}
    printf("REDUCED: %zu -> %zu bytes; %s preserved; %" PRIu64 " trial executions; %s\nresults: %s\n",input.len,m.current.len,outcome_name(original),m.tries,m.stop,m.root);bytes_free(&m.current);rc=0;goto done;
fail:fprintf(stderr,"error: minimize: %s\n",error);
done:if(report)fclose(report);free(json);if(have_baseline)result_free(&baseline);result_free(&archived);bytes_free(&input);bytes_free(&source);if(dir>=0)close(dir);config_free(&c);return rc;
}

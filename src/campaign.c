#include "beryl/campaign.h"
#include "beryl/options.h"
#include "beryl/report.h"
#include "beryl/record_codec.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
typedef struct {char signature[65],directory[B_PATH];unsigned count;} Group;
uint64_t monotonic_ms(void) {struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t)<0)return 0;return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
static size_t evidence_bytes(const Result *r) {
    size_t n=r->out.len+r->err.len+r->output.len;
    for(size_t i=0;i<r->extra_count;i++)n+=r->extra[i].bytes.len;
    return n+(r->reference?evidence_bytes(r->reference):0);
}
static bool failing(Outcome o) {return o!=PASS&&o!=UNVERIFIED;}
int fuzz_cli(int argc,char **argv) {
    Config c;config_defaults(&c);const char *copybook=NULL,*mode="valid",*format="free",*packed_sign="f";uint64_t runs=0,budget=60000,byte_limit=64U*1024U*1024U;bool seeded=false;
    int rc=2,dir=-1;Bytes source={0},seed={0},expected={0};Layout layout;Layout *schema=NULL;char error[512]="";
    char *summary=NULL;size_t summary_len=0;FILE *f=NULL;Group *groups=NULL;
    for(int i=2;i<argc;i++) {
        const char *flag=argv[i];if(!strcmp(flag,"--help")){puts("Usage: beryl fuzz --exec PATH [--reference PATH|--expected FILE] --copybook FILE --runs N --seed N --results NEW-DIR\n  --mode valid|invalid|raw; raw requires --input FILE; optional --input seeds field mutation\n  --budget-ms N (60000), --max-total-bytes N (67108864), standard run limits/arguments\n  --unsigned-packed-sign f|c selects generated unsigned COMP-3 signs (default f).\n  Runs: 1..10000. Every failing case is rerun before reproducible signature grouping.");rc=0;goto done;}
        if(i+1==argc){snprintf(error,sizeof error,"%s requires a value",flag);goto fail;}const char *v=argv[++i];
        if(!strcmp(flag,"--copybook"))copybook=v;
        else if(!strcmp(flag,"--format"))format=v;
        else if(!strcmp(flag,"--unsigned-packed-sign"))packed_sign=v;
        else if(!strcmp(flag,"--mode"))mode=v;
        else if(!strcmp(flag,"--runs")){if(parse_u64(v,10000,&runs)<0||!runs){strcpy(error,"--runs must be 1..10000");goto fail;}}
        else if(!strcmp(flag,"--seed")){if(parse_u64(v,UINT64_MAX,&c.seed)<0){strcpy(error,"--seed requires uint64");goto fail;}seeded=true;}
        else if(!strcmp(flag,"--budget-ms")){if(parse_u64(v,600000,&budget)<0||!budget){strcpy(error,"--budget-ms must be 1..600000");goto fail;}}
        else if(!strcmp(flag,"--max-total-bytes")){if(parse_u64(v,1073741824,&byte_limit)<0||!byte_limit){strcpy(error,"--max-total-bytes must be 1..1073741824");goto fail;}}
        else if(config_option(&c,flag,v)<0){snprintf(error,sizeof error,"invalid or unknown flag/value: %s",flag);goto fail;}
    }
    if(!runs||!seeded){strcpy(error,"fuzz requires --runs and --seed");goto fail;}
    if(layout_set_unsigned_packed_sign(NULL,packed_sign,error)<0)goto fail;
    if(strcmp(mode,"valid")&&strcmp(mode,"invalid")&&strcmp(mode,"raw")){strcpy(error,"--mode must be valid, invalid or raw");goto fail;}
    if(config_resolve(&c,error)<0)goto fail;
    if(copybook){if(read_bytes(copybook,B_ENV_LIMIT,&source)<0||source.truncated){strcpy(error,"cannot read --copybook (maximum 1 MiB)");goto fail;}if(copybook_parse_format((char *)source.data,source.len,format,&layout,error)<0)goto fail;schema=&layout;hash_bytes(source.data,source.len,c.layout_hash);}
    if(!schema&&strcmp(mode,"raw")){strcpy(error,"--copybook is required for valid/invalid mutation");goto fail;}
    if(schema&&layout_set_unsigned_packed_sign(&layout,packed_sign,error)<0)goto fail;
    if(*c.input){if(read_bytes(c.input,c.max_input,&seed)<0||seed.truncated){strcpy(error,"cannot read bounded --input seed");goto fail;}}
    if(c.has_expected&&(read_bytes(c.expected,c.max_capture,&expected)<0||expected.truncated)){strcpy(error,"cannot read bounded --expected");goto fail;}
    if(!strcmp(mode,"raw")&&!seed.data){strcpy(error,"raw mutation requires --input FILE");goto fail;}
    if(schema&&strcmp(mode,"raw")&&seed.data&&record_validate(schema,seed.data,seed.len,error)<0)goto fail;
    if((schema&&schema->size>=c.max_input)||seed.len>=c.max_input){strcpy(error,"mutation needs --max-input-bytes larger than seed/layout (one insertion byte)");goto fail;}
    dir=new_directory(c.results);if(dir<0){snprintf(error,sizeof error,"--results must be new and writable: %s",strerror(errno));goto fail;}
    char root[B_PATH];if(!realpath(c.results,root)){strcpy(error,"cannot resolve results directory");goto fail;}
    if(source.data&&copy_to(dir,"copybook.cpy",&source)<0){strcpy(error,"cannot archive copybook");goto fail;}
    if(seed.data&&copy_to(dir,"seed.bin",&seed)<0){strcpy(error,"cannot archive seed");goto fail;}
    if(c.has_expected&&(copy_to(dir,"expected.bin",&expected)<0||path_join(c.expected,root,"expected.bin")<0)){strcpy(error,"cannot archive golden oracle");goto fail;}
    size_t extra_snapshot=0;
    for(unsigned kind=0;kind<2;kind++)for(size_t i=0;i<(kind?c.output_count:c.input_count);i++) {
        FileSpec *spec=kind?&c.outputs[i]:&c.inputs[i];if(kind&&!spec->oracle)continue;Bytes bytes={0};char name[64];snprintf(name,sizeof name,"seed-%s-%02zu.bin",kind?"expected":"input",i);
        if(read_bytes(spec->path,kind?c.max_capture:c.max_input,&bytes)<0||bytes.truncated||copy_to(dir,name,&bytes)<0||path_join(spec->path,root,name)<0){bytes_free(&bytes);strcpy(error,"cannot snapshot additional campaign fixture");goto fail;}
        extra_snapshot+=bytes.len;bytes_free(&bytes);
    }
    groups=calloc((size_t)runs,sizeof *groups);f=open_memstream(&summary,&summary_len);if(!groups||!f){strcpy(error,"out of memory starting campaign");goto fail;}
    c.seeded=true;uint64_t state=c.seed,start=monotonic_ms();size_t completed=0,group_count=0,flaky=0,unconfirmed=0,failures=0,total=source.len+seed.len+expected.len+extra_snapshot;const char *stop="complete";
    fprintf(f,"{\"schema\":\"beryl-campaign-1\",\"version\":\"" BERYL_VERSION "\",\"seed\":\"%" PRIu64 "\",\"prng\":\"splitmix64-v1\",\"mode\":",c.seed);json_string(f,mode);fputs(",\"source_format\":",f);json_string(f,format);fputs(",\"unsigned_packed_sign\":",f);json_string(f,packed_sign);fprintf(f,",\"requested_runs\":%" PRIu64 ",\"budget_ms\":%" PRIu64 ",\"max_total_bytes\":%" PRIu64 ",\"cases\":[\n",runs,budget,byte_limit);
    for(size_t i=0;i<(size_t)runs;i++) {
        uint64_t elapsed=monotonic_ms()-start;if(elapsed>=budget){stop="time-budget";break;}if(total>=byte_limit){stop="artifact-budget";break;}
        Bytes record={0};if(mutate_record(schema,&seed,mode,i,&state,&record,c.mutation,error)<0)goto fail;
        char case_name[64],case_root[B_PATH];snprintf(case_name,sizeof case_name,"run-%06zu",i);if(path_join(case_root,root,case_name)<0){bytes_free(&record);strcpy(error,"case path too long");goto fail;}
        int case_dir=new_directory(case_root);if(case_dir<0){bytes_free(&record);strcpy(error,"cannot create case directory");goto fail;}
        size_t record_len=record.len;total+=record_len;
        int written=copy_to(case_dir,"input.bin",&record);close(case_dir);bytes_free(&record);if(written<0){strcpy(error,"cannot save mutated input");goto fail;}
        if(path_join(c.input,case_root,"input.bin")<0||path_join(c.results,case_root,"results")<0){strcpy(error,"case path too long");goto fail;}
        c.case_index=i;unsigned configured=c.timeout_ms;uint64_t remaining=(budget-elapsed)/(*c.reference?4U:2U);if(!remaining)remaining=1;if(c.timeout_ms>remaining)c.timeout_ms=(unsigned)remaining;
        Result result;(void)execute_case(&c,&result);char signature[65];failure_signature(&result,signature);bool reproduced=false;char confirmation[B_PATH]="";
        size_t fixture_bytes=(1U+(*c.reference?1U:0U))*(record_len+c.environment.len+extra_snapshot)+expected.len;
        total+=evidence_bytes(&result)+fixture_bytes;completed++;
        if(failing(result.outcome)) {
            failures++;
            if(result.outcome==HARNESS_ERROR||result.outcome==EXEC_ERROR){stop="execution-error";}
            else if(monotonic_ms()-start<budget&&total<byte_limit) {
                Result again;(void)execute_case(&c,&again);char other[65];failure_signature(&again,other);reproduced=!strcmp(signature,other);strcpy(confirmation,again.directory);total+=evidence_bytes(&again)+fixture_bytes;result_free(&again);
            }
            if(reproduced){size_t j;for(j=0;j<group_count&&strcmp(groups[j].signature,signature);j++){}if(j==group_count){strcpy(groups[j].signature,signature);strcpy(groups[j].directory,result.directory);group_count++;}groups[j].count++;}
            else if(strcmp(stop,"execution-error")){if(*confirmation)flaky++;else unconfirmed++;}
        }
        fprintf(f,"%s{\"index\":%zu,\"classification\":\"%s\",\"input_sha256\":\"%s\",\"signature\":\"%s\",\"reproducible\":%s,\"mutation\":",i?",\n":"",i,outcome_name(result.outcome),result.input_hash,signature,reproduced?"true":"false");json_string(f,c.mutation);fputs(",\"directory\":",f);json_string(f,result.directory);fputs(",\"confirmation\":",f);json_string(f,confirmation);fputc('}',f);
        c.timeout_ms=configured;result_free(&result);if(!strcmp(stop,"execution-error"))break;
    }
    fprintf(f,"\n],\"completed_runs\":%zu,\"failures\":%zu,\"unstable_cases\":%zu,\"unconfirmed_cases\":%zu,\"captured_artifact_bytes\":%zu,\"stop_reason\":",completed,failures,flaky,unconfirmed,total);json_string(f,stop);fputs(",\"groups\":[",f);
    for(size_t i=0;i<group_count;i++){fprintf(f,"%s{\"signature\":\"%s\",\"count\":%u,\"example\":",i?",":"",groups[i].signature,groups[i].count);json_string(f,groups[i].directory);fputc('}',f);}fputs("]}\n",f);
    {int bad=ferror(f);if(fclose(f))bad=1;f=NULL;if(bad||write_at(dir,"campaign.json",summary,summary_len)<0){strcpy(error,"cannot save campaign.json");goto fail;}}
    printf("CAMPAIGN: %zu/%" PRIu64 " runs, %zu failures, %zu reproducible signature groups, %zu unstable; %s\nresults: %s\n",completed,runs,failures,group_count,flaky,stop,root);
    rc=strcmp(stop,"complete")?2:failures?1:0;goto done;
fail:fprintf(stderr,"error: fuzz: %s\n",error);
done:if(f)fclose(f);free(summary);free(groups);bytes_free(&source);bytes_free(&seed);bytes_free(&expected);if(dir>=0)close(dir);config_free(&c);return rc;
}

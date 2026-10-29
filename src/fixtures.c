#include "beryl/fixtures.h"
#include "beryl/options.h"
#include "beryl/report.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void artifact(char name[64],const char *kind,size_t index){snprintf(name,64,"%s-%02zu.bin",kind,index);}
int fixtures_option(Config *c,const char *flag,const char *v) {
    bool input=!strcmp(flag,"--file");if(!input&&strcmp(flag,"--output"))return -2;
    size_t *count=input?&c->input_count:&c->output_count;if(*count>=B_EXTRA_FILES)return -1;
    const char *eq=strchr(v,'=');size_t n=eq?(size_t)(eq-v):strlen(v);FileSpec *s=input?&c->inputs[*count]:&c->outputs[*count];
    if(!n||n>=sizeof s->name||(input&&!eq)||(eq&&(!eq[1]||strlen(eq+1)>=sizeof s->path)))return -1;
    memcpy(s->name,v,n);s->name[n]=0;if(eq)strcpy(s->path,eq+1);s->oracle=!input&&eq;(*count)++;return 0;
}
int fixtures_validate(const Config *c,char error[512]) {
    if(c->input_count>B_EXTRA_FILES||c->output_count>B_EXTRA_FILES){strcpy(error,"at most eight additional inputs/outputs are supported");return -1;}
    const char *names[2+B_EXTRA_FILES*2];size_t count=0;names[count++]=c->input_name;names[count++]=c->output_name;
    for(size_t i=0;i<c->input_count;i++)names[count++]=c->inputs[i].name;
    for(size_t i=0;i<c->output_count;i++){names[count++]=c->outputs[i].name;if(*c->reference&&c->outputs[i].oracle){strcpy(error,"--output golden files and --reference are alternative oracles");return -1;}}
    for(size_t i=0;i<count;i++){if(!safe_name(names[i])){snprintf(error,512,"unsafe declared filename: %.400s",names[i]);return -1;}
        for(size_t j=0;j<i;j++){size_t a=strlen(names[i]),b=strlen(names[j]),n=a<b?a:b;if(!memcmp(names[i],names[j],n)&&(a==b||names[i][n]=='/'||names[j][n]=='/')){snprintf(error,512,"declared paths collide: %.200s and %.200s",names[i],names[j]);return -1;}}}
    return 0;
}
int fixtures_stage(const Config *c,Result *r,int archive,int work,size_t total,Bytes expected[B_EXTRA_FILES],char error[512]) {
    for(size_t i=0;i<c->input_count;i++) {
        Bytes bytes={0};char name[64];artifact(name,"input",i);
        if(total>c->max_input||read_bytes(c->inputs[i].path,c->max_input-total,&bytes)<0||bytes.truncated){if(bytes.truncated||total>c->max_input)errno=EFBIG;bytes_free(&bytes);snprintf(error,512,"additional input %.300s exceeds total input cap or cannot be read",c->inputs[i].name);return -1;}
        total+=bytes.len;hash_bytes(bytes.data,bytes.len,r->extra_input_hash[i]);int rc=copy_to(archive,name,&bytes);if(!rc)rc=copy_to(work,c->inputs[i].name,&bytes);bytes_free(&bytes);
        if(rc<0){snprintf(error,512,"cannot stage input %.300s",c->inputs[i].name);return -1;}
    }
    for(size_t i=0;i<c->output_count;i++) {
        char leaf[B_PATH];int parent=open_parent(work,c->outputs[i].name,true,leaf);if(parent<0){snprintf(error,512,"cannot prepare output %.300s",c->outputs[i].name);return -1;}close(parent);
        if(c->outputs[i].oracle){char name[64];artifact(name,"expected",i);if(read_bytes(c->outputs[i].path,c->max_capture,&expected[i])<0||expected[i].truncated||copy_to(archive,name,&expected[i])<0){snprintf(error,512,"cannot archive bounded golden output %.300s",c->outputs[i].name);return -1;}hash_bytes(expected[i].data,expected[i].len,r->extra_expected_hash[i]);}
    }
    return 0;
}
int fixtures_capture(const Config *c,Result *r,int work,char error[512]) {
    for(size_t i=0;i<c->output_count;i++){FileCapture *o=&r->extra[i];if(read_at(work,c->outputs[i].name,c->max_capture,&o->bytes)<0){if(errno==ENOENT)o->missing=true;else{snprintf(error,512,"cannot safely capture output %.300s",c->outputs[i].name);return -1;}}else hash_bytes(o->bytes.data,o->bytes.len,o->hash);}return 0;
}
void fixtures_golden(const Config *c,Result *r,const Bytes expected[B_EXTRA_FILES]) {
    if(r->outcome!=PASS&&r->outcome!=UNVERIFIED)return;
    for(size_t i=0;i<c->output_count;i++) {
        const FileCapture *o=&r->extra[i];if(o->missing){r->outcome=FAIL_ASSERTION;snprintf(r->diagnostic,sizeof r->diagnostic,"missing required output file: %.350s",c->outputs[i].name);return;}
        if(c->outputs[i].oracle){if(o->bytes.truncated){r->outcome=FAIL_ASSERTION;snprintf(r->diagnostic,sizeof r->diagnostic,"output truncated: %.350s; raise --max-capture-bytes",c->outputs[i].name);return;}
            size_t n=o->bytes.len<expected[i].len?o->bytes.len:expected[i].len,at=0;while(at<n&&o->bytes.data[at]==expected[i].data[at])at++;
            if(at<n||o->bytes.len!=expected[i].len){r->outcome=FAIL_ASSERTION;snprintf(r->diagnostic,sizeof r->diagnostic,"binary output difference in %.300s at offset %zu (expected %zu bytes, actual %zu)",c->outputs[i].name,at,expected[i].len,o->bytes.len);return;}
        }
    }
    if(r->outcome==UNVERIFIED)for(size_t i=0;i<c->output_count;i++)if(c->outputs[i].oracle){r->outcome=PASS;strcpy(r->diagnostic,"exit status and declared golden output files match");break;}
}
int fixtures_reference(const Config *c,Result *r,const Result *ref) {
    if(c->comparison==1)return 0;
    for(size_t i=0;i<c->output_count;i++){const FileCapture *a=&r->extra[i],*b=&ref->extra[i];
        if(a->bytes.truncated||b->bytes.truncated){r->outcome=HARNESS_ERROR;snprintf(r->diagnostic,sizeof r->diagnostic,"comparison capture truncated: %.350s",c->outputs[i].name);return -1;}
        if(a->missing&&b->missing){r->outcome=FAIL_ASSERTION;snprintf(r->diagnostic,sizeof r->diagnostic,"missing required output in both versions: %.350s",c->outputs[i].name);return -1;}
        if(a->missing!=b->missing||!bytes_equivalent(&a->bytes,&b->bytes,c->normalize_crlf))r->differences|=2;
    }
    return 0;
}
int fixtures_save(const Config *c,const Result *r,int archive) {
    for(size_t i=0;i<c->output_count;i++)if(!r->extra[i].missing&&r->extra[i].bytes.data){char name[64];artifact(name,"output",i);if(copy_to(archive,name,&r->extra[i].bytes)<0)return -1;}
    return 0;
}
static void hex(FILE *f,const char *key,const char *value,size_t index){fprintf(f,"%s%zu=",key,index);for(const unsigned char *s=(const unsigned char *)value;*s;s++)fprintf(f,"%02x",(unsigned)*s);fputc('\n',f);}
void fixtures_manifest(FILE *f,const Config *c,const Result *r) {
    fprintf(f,"extra_inputs=%zu\nextra_outputs=%zu\n",c->input_count,c->output_count);
    for(size_t i=0;i<c->input_count;i++){hex(f,"in_name_",c->inputs[i].name,i);fprintf(f,"in_hash_%zu=%s\n",i,r->extra_input_hash[i]);}
    for(size_t i=0;i<c->output_count;i++){hex(f,"out_name_",c->outputs[i].name,i);fprintf(f,"out_oracle_%zu=%d\nout_expected_%zu=%s\nout_hash_%zu=%s\nout_missing_%zu=%d\nout_truncated_%zu=%d\n",i,c->outputs[i].oracle,i,r->extra_expected_hash[i],i,r->extra[i].hash,i,r->extra[i].missing,i,r->extra[i].bytes.truncated);}
}
static int index_key(const char *key,const char *prefix,size_t *index){size_t n=strlen(prefix);if(strncmp(key,prefix,n))return 0;uint64_t value;if(parse_u64(key+n,B_EXTRA_FILES-1,&value)<0)return -1;*index=(size_t)value;return 1;}
static int unhex(const char *value,char *out,size_t size){size_t n=strlen(value);if(n%2||n/2>=size)return -1;for(size_t i=0;i<n;i+=2){if(!strchr("0123456789abcdef",value[i])||!strchr("0123456789abcdef",value[i+1]))return -1;char pair[3]={value[i],value[i+1],0};unsigned long v=strtoul(pair,NULL,16);if(!v)return -1;out[i/2]=(char)v;}out[n/2]=0;return 0;}
int fixtures_parse(Config *c,const char *key,const char *value) {
    uint64_t n;size_t index;
    if(!strcmp(key,"extra_inputs")||!strcmp(key,"extra_outputs")){if(parse_u64(value,B_EXTRA_FILES,&n)<0)return -1;if(!strcmp(key,"extra_inputs"))c->input_count=(size_t)n;else c->output_count=(size_t)n;return 1;}
    const char *prefixes[]={"in_name_","in_hash_","out_name_","out_oracle_","out_expected_"};
    for(unsigned i=0;i<5;i++){int found=index_key(key,prefixes[i],&index);if(found<0)return -1;if(!found)continue;
        bool input=i<2;FileSpec *s=input?&c->inputs[index]:&c->outputs[index];if(index>=(input?c->input_count:c->output_count))return -1;
        if(i==0||i==2){if(unhex(value,s->name,sizeof s->name)<0)return -1;s->seen|=1;}
        else if(i==3){if(parse_u64(value,1,&n)<0)return -1;s->oracle=n!=0;s->seen|=4;}
        else {if(strlen(value)>64)return -1;strcpy(s->hash,value);s->seen|=2;}return 1;
    }return 0;
}
int fixtures_replay(Config *c,int dir,const char *root,size_t total) {
    char error[512];if(fixtures_validate(c,error)<0)return -1;
    for(unsigned kind=0;kind<2;kind++)for(size_t i=0;i<(kind?c->output_count:c->input_count);i++){
        FileSpec *s=kind?&c->outputs[i]:&c->inputs[i];if(s->seen!=(kind?7U:3U))return -1;if(kind&&!s->oracle){if(*s->hash)return -1;continue;}
        Bytes b={0};char name[64],hash[65];artifact(name,kind?"expected":"input",i);size_t cap=kind?c->max_capture:c->max_input-total;
        if(read_at(dir,name,cap,&b)<0||b.truncated){bytes_free(&b);return -1;}hash_bytes(b.data,b.len,hash);if(!kind)total+=b.len;bytes_free(&b);
        if(strcmp(hash,s->hash)||path_join(s->path,root,name)<0)return -1;
    }return 0;
}
void fixtures_json(FILE *f,const Config *c,const Result *r) {
    fputs(",\n  \"additional_inputs\":[",f);for(size_t i=0;i<c->input_count;i++){if(i)fputc(',',f);fputs("{\"name\":",f);json_string(f,c->inputs[i].name);fprintf(f,",\"sha256\":\"%s\"}",r->extra_input_hash[i]);}
    fputs("],\n  \"additional_outputs\":[",f);for(size_t i=0;i<c->output_count;i++){if(i)fputc(',',f);fputs("{\"name\":",f);json_string(f,c->outputs[i].name);fprintf(f,",\"sha256\":\"%s\",\"expected_sha256\":\"%s\",\"oracle\":%s,\"captured_bytes\":%zu,\"missing\":%s,\"truncated\":%s}",r->extra[i].hash,r->extra_expected_hash[i],c->outputs[i].oracle?"true":"false",r->extra[i].bytes.len,r->extra[i].missing?"true":"false",r->extra[i].bytes.truncated?"true":"false");}fputc(']',f);
}
int fixtures_result_field(Result *r,const char *key,const char *value) {
    uint64_t n;size_t index;
    if(!strcmp(key,"extra_outputs")){if(parse_u64(value,B_EXTRA_FILES,&n)<0)return -1;r->extra_count=(size_t)n;return 1;}
    const char *prefixes[]={"out_hash_","out_missing_","out_truncated_"};
    for(unsigned i=0;i<3;i++){int found=index_key(key,prefixes[i],&index);if(found<0)return -1;if(!found)continue;if(index>=r->extra_count)return -1;
        if(!i){if(strlen(value)>64)return -1;strcpy(r->extra[index].hash,value);}else{if(parse_u64(value,1,&n)<0)return -1;if(i==1)r->extra[index].missing=n!=0;else r->extra[index].bytes.truncated=n!=0;}return 1;}return 0;
}
int fixtures_saved_capture(int dir,size_t cap,Result *r) {
    for(size_t i=0;i<r->extra_count;i++)if(!r->extra[i].missing){char name[64],hash[65];artifact(name,"output",i);bool truncated=r->extra[i].bytes.truncated;if(read_at(dir,name,cap,&r->extra[i].bytes)<0||r->extra[i].bytes.truncated)return -1;hash_bytes(r->extra[i].bytes.data,r->extra[i].bytes.len,hash);if(strcmp(hash,r->extra[i].hash))return -1;r->extra[i].bytes.truncated=truncated;}
    return 0;
}

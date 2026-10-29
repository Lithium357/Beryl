#include "beryl/campaign.h"
#include "beryl/generator.h"
#include "beryl/record_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
uint64_t monotonic_ms(void);
int mutate_record(const Layout *layout,const Bytes *seed,const char *mode,size_t index,uint64_t *state,Bytes *record,char operation[256],char error[512]) {
    memset(record,0,sizeof *record);
    size_t len=!strcmp(mode,"raw")?seed->len:layout?layout->size:0;
    if(len>67108864U){strcpy(error,"mutation input exceeds 64 MiB");return -1;}
    record->data=malloc(len+1);if(!record->data){strcpy(error,"out of memory mutating record");return -1;}record->len=len;
    if(!strcmp(mode,"raw")) {
        if(seed->len)memcpy(record->data,seed->data,seed->len);
        if(!index){strcpy(operation,"raw-seed");return 0;}
        unsigned choice=(unsigned)(generator_next(state)%3);
        size_t at=(size_t)(generator_next(state)%(len?len:1));
        if(choice==0&&len){unsigned bit=(unsigned)(generator_next(state)%8);record->data[at]^=(unsigned char)(1U<<bit);snprintf(operation,256,"raw-flip:%zu:%u",at,bit);}
        else if(choice==1&&len){memmove(record->data+at,record->data+at+1,len-at-1);record->len--;snprintf(operation,256,"raw-delete:%zu",at);}
        else {memmove(record->data+at+1,record->data+at,len-at);record->data[at]=(unsigned char)generator_next(state);record->len++;snprintf(operation,256,"raw-insert:%zu",at);}
        return 0;
    }
    if(!layout){strcpy(error,"valid/invalid modes require --copybook");goto fail;}
    if(generate_record(layout,index,state,record->data,error)<0)goto fail;
    if(seed&&seed->data) {
        if(seed->len!=layout->size){strcpy(error,"seed input length differs from copybook layout");goto fail;}
        if(record_validate(layout,seed->data,seed->len,error)<0)goto fail;
        size_t field=(size_t)(generator_next(state)%layout->count);const Field *f=&layout->fields[field];
        unsigned char temporary[1024];if(f->size>sizeof temporary){strcpy(error,"field exceeds mutation bound");goto fail;}
        memcpy(temporary,record->data+f->offset,f->size);memcpy(record->data,seed->data,seed->len);memcpy(record->data+f->offset,temporary,f->size);
        snprintf(operation,256,"valid-field:%.240s",f->name);
    } else snprintf(operation,256,"valid-generated:%zu",index);
    if(!strcmp(mode,"invalid")) {
        unsigned choice=(unsigned)(generator_next(state)%3);
        if(choice==0){record->len--;strcpy(operation,"invalid-truncate");}
        else if(choice==1){record->data[record->len++]=0;strcpy(operation,"invalid-append-nul");}
        else {
            size_t i;for(i=0;i<layout->count&&layout->fields[i].storage==B_TEXT;i++){}
            if(i==layout->count){record->len--;strcpy(operation,"invalid-text-truncate");}
            else {record->data[layout->fields[i].offset]=0xff;snprintf(operation,256,"invalid-digit:%.230s",layout->fields[i].name);}
        }
    }
    return 0;
fail:bytes_free(record);return -1;
}
static unsigned assertion_kind(const Result *r) {
    if(r->outcome!=FAIL_ASSERTION)return 0;
    if(strstr(r->diagnostic,"exit status"))return 1;
    if(strstr(r->diagnostic,"missing required"))return 2;
    if(strstr(r->diagnostic,"truncated"))return 3;
    if(strstr(r->diagnostic,"binary"))return 4;
    return 5;
}
void failure_signature(const Result *r,char hash[65]) {
    char out[65],err[65],stream[65],ref[65]="",refstream[65]="",referr[65]="";hash_bytes(r->output.data,r->output.len,out);hash_bytes(r->err.data,r->err.len,err);hash_bytes(r->out.data,r->out.len,stream);
    if(r->reference)hash_bytes(r->reference->output.data,r->reference->output.len,ref);
    if(r->reference){hash_bytes(r->reference->out.data,r->reference->out.len,refstream);hash_bytes(r->reference->err.data,r->reference->err.len,referr);}
    char buffer[1024];int n=snprintf(buffer,sizeof buffer,"%d:%d:%d:%u:%u:%d:%d:%d:%d:%d:%s:%s:%s:%s:%s:%s",r->outcome,r->exit_code,r->signal_number,r->differences,assertion_kind(r),r->missing_output,r->output.truncated,r->err.truncated,r->reference?r->reference->exit_code:-1,r->reference?r->reference->signal_number:0,out,err,ref,stream,refstream,referr);
    hash_bytes(buffer,(size_t)n,hash);
}
bool failure_equivalent(const Result *a,const Result *b,bool signature) {
    if(a->outcome!=b->outcome||a->signal_number!=b->signal_number||a->differences!=b->differences||assertion_kind(a)!=assertion_kind(b))return false;
    if(!!a->reference!=!!b->reference)return false;
    if(a->reference&&(a->reference->signal_number!=b->reference->signal_number||((a->differences&1)&&(a->exit_code!=b->exit_code||a->reference->exit_code!=b->reference->exit_code))))return false;
    if(a->outcome==FAIL_ASSERTION&&assertion_kind(a)==1&&a->exit_code!=b->exit_code)return false;
    if(signature){char x[65],y[65];failure_signature(a,x);failure_signature(b,y);return !strcmp(x,y);}
    return true;
}

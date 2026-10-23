#include "beryl/beryl.h"
#include <stdio.h>
#include <string.h>
const char *outcome_name(Outcome o) {
    static const char *names[]={"PASS","FAIL_ASSERTION","CRASH","TIMEOUT","EXEC_ERROR","HARNESS_ERROR","UNVERIFIED","REGRESSION_CANDIDATE"};
    return names[o];
}
static int equivalent(const Bytes *a,const Bytes *b,bool crlf) {
    size_t i=0,j=0;
    while(i<a->len && j<b->len) {
        if(crlf && a->data[i]=='\r' && i+1<a->len && a->data[i+1]=='\n')i++;
        if(crlf && b->data[j]=='\r' && j+1<b->len && b->data[j+1]=='\n')j++;
        if(a->data[i++]!=b->data[j++])return 0;
    }
    return i==a->len && j==b->len;
}
void compare_reference(const Config *c,Result *r,const Result *ref) {
    if(r->outcome==HARNESS_ERROR || r->outcome==EXEC_ERROR)return;
    if(ref->outcome==HARNESS_ERROR || ref->outcome==EXEC_ERROR) {
        r->outcome=ref->outcome;snprintf(r->diagnostic,sizeof r->diagnostic,"reference failed: %.450s",ref->diagnostic);return;
    }
    if(ref->outcome==TIMEOUT || r->outcome==TIMEOUT) {
        r->outcome=TIMEOUT;strcpy(r->diagnostic,"reference or candidate timed out; equality is unestablished");return;
    }
    if(c->comparison!=1 && (r->output.truncated || ref->output.truncated ||
       (c->comparison==2 && (r->out.truncated || r->err.truncated || ref->out.truncated || ref->err.truncated)))) {
        r->outcome=HARNESS_ERROR;strcpy(r->diagnostic,"comparison capture truncated; raise --max-capture-bytes");return;
    }
    if(r->exit_code!=ref->exit_code || r->signal_number!=ref->signal_number)r->differences|=1;
    if(c->comparison!=1) {
        if(r->missing_output || ref->missing_output) {
            if(r->missing_output && ref->missing_output && !r->differences) {
                r->outcome=FAIL_ASSERTION;strcpy(r->diagnostic,"missing required output in both reference and candidate");return;
            }
            r->differences|=2;
        } else {
            if(!equivalent(&r->output,&ref->output,c->normalize_crlf))r->differences|=2;
        }
    }
    if(c->comparison==2) {
        if(!equivalent(&r->out,&ref->out,c->normalize_crlf))r->differences|=4;
        if(!equivalent(&r->err,&ref->err,c->normalize_crlf))r->differences|=8;
    }
    if(r->differences) {
        r->outcome=REGRESSION_CANDIDATE;
        snprintf(r->diagnostic,sizeof r->diagnostic,"reference difference: status=%d output=%d stdout=%d stderr=%d; review expected behavior",!!(r->differences&1),!!(r->differences&2),!!(r->differences&4),!!(r->differences&8));
    } else if(r->signal_number) {
        r->outcome=CRASH;strcpy(r->diagnostic,"both executions terminated by the same signal; matching crashes are not a successful test");
    } else {
        r->outcome=PASS;strcpy(r->diagnostic,"selected observables match reference; business correctness depends on the reference");
    }
}
void compare_result(const Config *c,Result *r,const Bytes *expected) {
    if(r->outcome!=UNVERIFIED)return;
    r->outcome=FAIL_ASSERTION;
    if(r->exit_code!=c->expected_exit) { snprintf(r->diagnostic,sizeof r->diagnostic,"exit status: expected %d, actual %d",c->expected_exit,r->exit_code);return; }
    if(r->missing_output) { snprintf(r->diagnostic,sizeof r->diagnostic,"missing required output file: %.350s",c->output_name);return; }
    if(!c->has_expected) { r->outcome=UNVERIFIED;strcpy(r->diagnostic,"output observed; no golden oracle supplied");return; }
    if(r->output.truncated) { snprintf(r->diagnostic,sizeof r->diagnostic,"output truncated at %zu bytes; cannot establish equality; raise --max-capture-bytes",c->max_capture);return; }
    size_t n=expected->len<r->output.len?expected->len:r->output.len;
    for(size_t i=0;i<n;i++)if(expected->data[i]!=r->output.data[i]) {
        snprintf(r->diagnostic,sizeof r->diagnostic,"binary difference at offset %zu: expected 0x%02x, actual 0x%02x (lengths %zu/%zu)",i,(unsigned)expected->data[i],(unsigned)r->output.data[i],expected->len,r->output.len);return;
    }
    if(expected->len!=r->output.len){snprintf(r->diagnostic,sizeof r->diagnostic,"binary length difference at offset %zu: expected %zu bytes, actual %zu",n,expected->len,r->output.len);return;}
    r->outcome=PASS;strcpy(r->diagnostic,"exit status and required output match golden bytes");
}

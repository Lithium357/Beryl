#include "beryl/beryl.h"
#include <stdio.h>
#include <string.h>
const char *outcome_name(Outcome o) {
    static const char *names[]={"PASS","FAIL_ASSERTION","CRASH","TIMEOUT","EXEC_ERROR","HARNESS_ERROR","UNVERIFIED"};
    return names[o];
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

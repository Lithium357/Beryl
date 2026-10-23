#include "beryl/beryl.h"
#include "beryl/generator.h"
#include "beryl/campaign.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void help(void) {
    puts("Beryl " BERYL_VERSION " — Linux batch regression runner\n"
         "Usage: beryl run --exec PATH --input FILE [options]\n"
         "       beryl replay RESULT-DIRECTORY [--exec PATH] [--results DIR]\n"
         "       beryl generate --copybook FILE --count N --seed N --out DIR [--malformed]\n"
         "       beryl fuzz --exec PATH --copybook FILE --runs N --seed N --results NEW-DIR\n"
         "       beryl --help | --version\n\n"
         "  --input-name NAME       relative input name (INPUT.DAT)\n"
         "  --output-name NAME      required output name (OUTPUT.DAT)\n"
         "  --expected FILE         binary golden output; otherwise UNVERIFIED\n"
         "  --reference PATH        compare with another executable in its own workspace\n"
         "  --compare POLICY        output (default), status, or all (includes streams)\n"
         "  --normalize-crlf 0|1    explicit CRLF-to-LF comparison policy (0)\n"
         "  --expected-exit N       expected exit status, 0..255 (0)\n"
         "  --arg VALUE             literal target argument; repeat up to 64 times\n"
         "  --results DIR           archive root (.beryl/results)\n"
         "  --timeout-ms N          wall limit, 1..600000 (2000)\n"
         "  --cpu-seconds N         CPU limit, 1..3600 (2)\n"
         "  --max-input-bytes N     input cap, 1..67108864 (1048576)\n"
         "  --max-capture-bytes N   per stream/output/golden cap (1048576)\n"
         "  --max-file-bytes N      child per-file limit (16777216)\n"
         "  --compiler-version TEXT compiler provenance if known\n\n"
         "Statuses: 0 PASS/UNVERIFIED; 1 assertion/crash/timeout; 2 usage/harness/exec error.\n"
         "Evidence includes raw bytes and the environment (may contain secrets).\n"
         "Resource caps are not a security sandbox; isolate untrusted targets with OS/container controls.");
}
static int string_option(char *dest,size_t size,const char *value) {
    if(strlen(value)>=size)return -1;
    strcpy(dest,value);return 0;
}
static int parse_number(const char *s,unsigned long max,unsigned long *n) {
    if(!*s)return -1;
    for(const char *p=s;*p;p++)if(*p<'0'||*p>'9')return -1;
    errno=0;char *end;*n=strtoul(s,&end,10);return errno || *end || *n>max ? -1:0;
}
int cli_main(int argc,char **argv) {
    if(argc>=2 && !strcmp(argv[1],"fuzz"))return fuzz_cli(argc,argv);
    if(argc>=2 && !strcmp(argv[1],"generate"))return generate_cli(argc,argv);
    if(argc==2 && !strcmp(argv[1],"--version")){puts("beryl " BERYL_VERSION);return 0;}
    if(argc==2 && !strcmp(argv[1],"--help")){help();return 0;}
    if(argc<2 || (strcmp(argv[1],"run") && strcmp(argv[1],"replay"))){fputs("error: choose run, replay or generate; see beryl --help\n",stderr);return 2;}
    if(argc==3 && !strcmp(argv[2],"--help")){help();return 0;}
    Config c;config_defaults(&c);int rc=2,index=2;bool replay=!strcmp(argv[1],"replay"),override=false,reference_override=false;
    char saved_hash[65]="";
    if(replay) {
        if(argc<3){fputs("error: replay needs a result directory; see beryl --help\n",stderr);goto done;}
        if(replay_load(argv[2],&c,saved_hash)<0){fprintf(stderr,"error: replay %s: invalid/incomplete manifest or archived digest mismatch\n",argv[2]);goto done;}index=3;
    }
    for(int i=index;i<argc;i++) {
        const char *flag=argv[i];
        if(!strcmp(flag,"--help")){help();rc=0;goto done;}
        if(i+1>=argc){fprintf(stderr,"error: %s requires a value; see beryl --help\n",flag);goto done;}
        const char *v=argv[++i];int bad=0;
        if(replay && strcmp(flag,"--exec") && strcmp(flag,"--reference") && strcmp(flag,"--results")){fprintf(stderr,"error: replay does not accept %s; only --exec, --reference and --results may override saved settings\n",flag);goto done;}
        if(!strcmp(flag,"--exec")){bad=string_option(c.target,sizeof c.target,v);override=true;}
        else if(!strcmp(flag,"--input"))bad=string_option(c.input,sizeof c.input,v);
        else if(!strcmp(flag,"--expected")){bad=string_option(c.expected,sizeof c.expected,v);c.has_expected=true;}
        else if(!strcmp(flag,"--reference")){bad=string_option(c.reference,sizeof c.reference,v);reference_override=true;}
        else if(!strcmp(flag,"--compare")) { if(!strcmp(v,"output"))c.comparison=0;else if(!strcmp(v,"status"))c.comparison=1;else if(!strcmp(v,"all"))c.comparison=2;else bad=1; }
        else if(!strcmp(flag,"--normalize-crlf")) { if(!strcmp(v,"0"))c.normalize_crlf=false;else if(!strcmp(v,"1"))c.normalize_crlf=true;else bad=1; }
        else if(!strcmp(flag,"--results"))bad=string_option(c.results,sizeof c.results,v);
        else if(!strcmp(flag,"--input-name"))bad=string_option(c.input_name,sizeof c.input_name,v);
        else if(!strcmp(flag,"--output-name"))bad=string_option(c.output_name,sizeof c.output_name,v);
        else if(!strcmp(flag,"--compiler-version"))bad=string_option(c.compiler,sizeof c.compiler,v);
        else if(!strcmp(flag,"--arg")) {
            if(c.argc>=B_ARGS || strlen(v)>=B_PATH)bad=1;
            else { c.args[c.argc]=strdup(v);if(!c.args[c.argc])bad=1;else c.argc++; }
        } else {
            const char *keys[]={"--expected-exit","--timeout-ms","--cpu-seconds","--max-input-bytes","--max-capture-bytes","--max-file-bytes"};
            unsigned j;for(j=0;j<6;j++)if(!strcmp(flag,keys[j]))break;
            if(j==6){fprintf(stderr,"error: unknown flag %s; see beryl --help\n",flag);goto done;}
            unsigned long n,max=j==0?255UL:j==1?600000UL:j==2?3600UL:67108864UL;
            if(parse_number(v,max,&n)<0 || (j && !n))bad=1;
            else if(j==0)c.expected_exit=(int)n;
            else if(j==1)c.timeout_ms=(unsigned)n;
            else if(j==2)c.cpu_seconds=(unsigned)n;
            else if(j==3)c.max_input=(size_t)n;
            else if(j==4)c.max_capture=(size_t)n;
            else c.max_file=(size_t)n;
        }
        if(bad){fprintf(stderr,"error: invalid or oversized value for %s; see beryl --help for limits\n",flag);goto done;}
    }
    if(!*c.target || !*c.input){fputs("error: run requires --exec and --input; see beryl run --help\n",stderr);goto done;}
    if(!safe_name(c.input_name)||!safe_name(c.output_name)||!strcmp(c.input_name,c.output_name)) { fputs("error: --input-name and --output-name must be distinct safe relative paths (no absolute paths, '.', '..', empty components or backslashes)\n",stderr);goto done; }
    char resolved[B_PATH];
    if(!realpath(c.target,resolved)){fprintf(stderr,"error: --exec %s: %s; provide an existing executable\n",c.target,strerror(errno));goto done;}
    strcpy(c.target,resolved);
    if(c.has_expected && *c.reference){fputs("error: --expected and --reference are alternative oracles\n",stderr);goto done;}
    if(*c.reference) {
        if(!realpath(c.reference,resolved)){fprintf(stderr,"error: --reference %s: %s\n",c.reference,strerror(errno));goto done;}
        strcpy(c.reference,resolved);
        if(replay && !reference_override) { char hash[65];if(hash_file(c.reference,hash)<0 || strcmp(hash,c.replay_reference_hash)){fputs("error: replay reference digest changed; select --reference PATH deliberately\n",stderr);goto done;} }
    } else if(c.comparison || c.normalize_crlf) { fputs("error: comparison policy requires --reference\n",stderr);goto done; }
    if(replay && !override) {
        char actual[65];if(hash_file(c.target,actual)<0 || strcmp(actual,saved_hash)){fputs("error: replay target digest changed; deliberately select --exec PATH to override\n",stderr);goto done;}
    }
    Result r; (void)execute_case(&c,&r);
    printf("%s: %s\n",outcome_name(r.outcome),r.diagnostic);
    if(*r.directory)printf("results: %s\n",r.directory);
    printf("exit=%d signal=%d duration_ms=%llu stdout_truncated=%d stderr_truncated=%d output_truncated=%d\n",r.exit_code,r.signal_number,(unsigned long long)r.duration_ms,r.out.truncated,r.err.truncated,r.output.truncated);
    rc=(r.outcome==PASS || r.outcome==UNVERIFIED)?0:(r.outcome==HARNESS_ERROR || r.outcome==EXEC_ERROR)?2:1;
    result_free(&r);
done:
    config_free(&c);return rc;
}

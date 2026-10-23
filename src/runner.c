#include "beryl/beryl.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static uint64_t now_ms(void) { struct timespec t; if(clock_gettime(CLOCK_MONOTONIC,&t)<0)return 0;return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000; }
static int pipe_cloexec(int p[2]) {
    if(pipe(p)<0)return -1;
    if(fcntl(p[0],F_SETFD,FD_CLOEXEC)<0 || fcntl(p[1],F_SETFD,FD_CLOEXEC)<0){int e=errno;close(p[0]);close(p[1]);errno=e;return -1;}return 0;
}
static void child_error(int fd,int e) { ssize_t ignored=write(fd,&e,sizeof e);(void)ignored;_exit(127); }
static void cap(int fd,Bytes *b,size_t max) {
    unsigned char buf[8192];
    /* Bounded work per poll iteration: a flooding child must not starve the timeout. */
    for(unsigned i=0;i<16;i++) {
        ssize_t n=read(fd,buf,sizeof buf);
        if(n<=0) { if(n<0 && errno==EINTR)continue;break; }
        size_t take=(size_t)n; if(take>max-b->len)take=max-b->len;
        memcpy(b->data+b->len,buf,take);b->len+=take;
        if(take<(size_t)n)b->truncated=true;
    }
}
int run_child(const Config *c,Result *r,int work) {
    int p[3][2]={{-1,-1},{-1,-1},{-1,-1}};
    char **env=NULL; size_t count=0;
    for(size_t i=0;i<c->environment.len;i++)if(!c->environment.data[i])count++;
    env=calloc(count+1,sizeof *env);if(!env)goto fail;
    size_t off=0;for(size_t i=0;i<count;i++){env[i]=(char *)c->environment.data+off;off+=strlen(env[i])+1;}
    r->out.data=malloc(c->max_capture+1);r->err.data=malloc(c->max_capture+1);
    if(!r->out.data || !r->err.data)goto fail;
    for(unsigned i=0;i<3;i++)if(pipe_cloexec(p[i])<0)goto fail;
    uint64_t start=now_ms(); pid_t pid=fork();if(pid<0)goto fail;
    if(!pid) {
        for(unsigned i=0;i<3;i++)close(p[i][0]);
        if(setpgid(0,0)<0 || fchdir(work)<0)child_error(p[2][1],errno);
        struct rlimit cpu={(rlim_t)c->cpu_seconds,(rlim_t)c->cpu_seconds};
        struct rlimit file={(rlim_t)c->max_file,(rlim_t)c->max_file};
        struct rlimit core={0,0}, mem={512U*1024U*1024U,512U*1024U*1024U};
        if(setrlimit(RLIMIT_CPU,&cpu)<0 || setrlimit(RLIMIT_FSIZE,&file)<0 || setrlimit(RLIMIT_CORE,&core)<0 || setrlimit(RLIMIT_AS,&mem)<0)child_error(p[2][1],errno);
        int in=open("/dev/null",O_RDONLY);
        if(in<0 || dup2(in,STDIN_FILENO)<0 || dup2(p[0][1],STDOUT_FILENO)<0 || dup2(p[1][1],STDERR_FILENO)<0)child_error(p[2][1],errno);
        if(in>2)close(in);
        close(p[0][1]);close(p[1][1]);
        char *argv[B_ARGS+2];argv[0]=(char *)c->target;
        for(size_t i=0;i<c->argc;i++)argv[i+1]=c->args[i];
        argv[c->argc+1]=NULL;
        execve(c->target,argv,env);child_error(p[2][1],errno);
    }
    free(env);env=NULL;
    /* Both parent and child attempt setpgid to close the scheduling race. */
    if(setpgid(pid,pid)<0 && errno!=EACCES && errno!=ESRCH) { kill(pid,SIGKILL);waitpid(pid,NULL,0);goto fail; }
    for(unsigned i=0;i<3;i++){close(p[i][1]);p[i][1]=-1;}
    for(unsigned i=0;i<2;i++)if(fcntl(p[i][0],F_SETFL,O_NONBLOCK)<0){kill(-pid,SIGKILL);waitpid(pid,NULL,0);goto fail;}
    int status=0;bool timed=false;
    for(;;) {
        cap(p[0][0],&r->out,c->max_capture);cap(p[1][0],&r->err,c->max_capture);
        pid_t done=waitpid(pid,&status,WNOHANG);
        if(done==pid)break;
        if(done<0 && errno!=EINTR){kill(-pid,SIGKILL);goto fail;}
        if(now_ms()-start>=c->timeout_ms){timed=true;kill(-pid,SIGKILL);while(waitpid(pid,&status,0)<0 && errno==EINTR){}break;}
        struct pollfd fds[2]={{p[0][0],POLLIN,0},{p[1][0],POLLIN,0}};
        (void)poll(fds,2,10);
    }
    /* Also terminate descendants left behind after an early leader exit. */
    (void)kill(-pid,SIGKILL);
    for(unsigned i=0;i<2;i++)cap(p[i][0],i?&r->err:&r->out,c->max_capture);
    r->duration_ms=now_ms()-start;
    ssize_t n;do { n=read(p[2][0],&r->exec_errno,sizeof r->exec_errno); }while(n<0 && errno==EINTR);
    if(n<0 || (n>0 && n!=(ssize_t)sizeof r->exec_errno))goto fail;
    for(unsigned i=0;i<3;i++){close(p[i][0]);p[i][0]=-1;}
    if(WIFEXITED(status))r->exit_code=WEXITSTATUS(status);
    if(WIFSIGNALED(status))r->signal_number=WTERMSIG(status);
    if(timed) { r->outcome=TIMEOUT;snprintf(r->diagnostic,sizeof r->diagnostic,"wall timeout after %u ms; process group terminated",c->timeout_ms); }
    else if(r->exec_errno) { r->outcome=EXEC_ERROR;snprintf(r->diagnostic,sizeof r->diagnostic,"cannot execute %.300s: %s",c->target,strerror(r->exec_errno)); }
    else if(r->signal_number) { r->outcome=CRASH;snprintf(r->diagnostic,sizeof r->diagnostic,"target terminated by signal %d",r->signal_number); }
    return 0;
fail:
    { int e=errno;free(env);for(unsigned i=0;i<3;i++)for(unsigned j=0;j<2;j++)if(p[i][j]>=0)close(p[i][j]);errno=e;return -1; }
}

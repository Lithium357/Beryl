#include "beryl/beryl.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void bytes_free(Bytes *b) { free(b->data); memset(b, 0, sizeof *b); }
int path_join(char out[B_PATH], const char *a, const char *b) {
    int n = snprintf(out, B_PATH, "%s/%s", a, b);
    if (n < 0 || n >= B_PATH) { errno = ENAMETOOLONG; return -1; } return 0;
}
int safe_name(const char *name) {
    if (!name || !*name || *name == '/' || strlen(name) >= B_PATH) return 0;
    const char *p = name;
    while (*p) {
        const char *q = strchr(p, '/'); size_t n = q ? (size_t)(q-p) : strlen(p);
        if (!n || (n == 1 && *p == '.') || (n == 2 && p[0] == '.' && p[1] == '.')) return 0;
        for (size_t i = 0; i < n; i++)
            if ((unsigned char)p[i] < 32 || p[i] == '\\' || p[i] == ':') return 0;
        if (!q) break;
        p = q + 1; if (!*p) return 0;
    } return 1;
}
/* Every component is opened relative to an already-open directory; never follow symlinks. */
int open_directory(const char *path, bool create) {
    char buf[B_PATH];
    if (!path || !*path || strlen(path) >= sizeof buf) { errno = EINVAL; return -1; }
    strcpy(buf, path);
    int fd = open(path[0] == '/' ? "/" : ".", O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if (fd < 0) return -1;
    char *state = NULL;
    for (char *p = strtok_r(buf, "/", &state); p; p = strtok_r(NULL, "/", &state)) {
        if (!strcmp(p, ".")) continue;
        if (!strcmp(p, "..")) { close(fd); errno = EINVAL; return -1; }
        if (create && mkdirat(fd, p, 0700) < 0 && errno != EEXIST) { close(fd); return -1; }
        int next = openat(fd, p, O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        int e = errno; close(fd); if (next < 0) { errno = e; return -1; } fd = next;
    } return fd;
}
int open_parent(int root, const char *name, bool create, char leaf[B_PATH]) {
    if (!safe_name(name)) { errno = EINVAL; return -1; }
    char buf[B_PATH]; strcpy(buf, name);
    char *last = strrchr(buf, '/'); int fd = fcntl(root, F_DUPFD_CLOEXEC, 3);
    if (fd < 0) return -1;
    if (!last) { strcpy(leaf, name); return fd; }
    *last = 0; strcpy(leaf, last+1); char *state = NULL;
    for (char *p = strtok_r(buf, "/", &state); p; p = strtok_r(NULL, "/", &state)) {
        if (create && mkdirat(fd,p,0700)<0 && errno != EEXIST) { close(fd); return -1; }
        int next = openat(fd,p,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        int e=errno; close(fd); if(next<0) { errno=e; return -1; } fd=next;
    } return fd;
}
static int read_fd(int fd, size_t limit, Bytes *b) {
    struct stat st;
    memset(b,0,sizeof *b);
    if (fstat(fd,&st)<0) return -1;
    if (!S_ISREG(st.st_mode)) { errno=EINVAL; return -1; }
    b->data=malloc(limit+1); if(!b->data) return -1;
    while(b->len < limit+1) {
        ssize_t n=read(fd,b->data+b->len,limit+1-b->len);
        if(n<0) { if(errno==EINTR) continue; bytes_free(b); return -1; }
        if(!n) break;
        b->len+=(size_t)n;
    }
    if(b->len>limit) { b->len=limit; b->truncated=true; }
    return 0;
}
int read_bytes(const char *path,size_t limit,Bytes *b) {
    /* Sources may be anywhere, but must be regular, non-symlink final components. */
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC); if(fd<0) return -1;
    int rc=read_fd(fd,limit,b), e=errno; close(fd); errno=e; return rc;
}
int read_at(int root,const char *path,size_t limit,Bytes *b) {
    char leaf[B_PATH]; int dir=open_parent(root,path,false,leaf); if(dir<0) return -1;
    int fd=openat(dir,leaf,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC); int e=errno; close(dir);
    if(fd<0) { errno=e; return -1; }
    int rc=read_fd(fd,limit,b); e=errno; close(fd); errno=e; return rc;
}
int write_at(int root,const char *name,const void *data,size_t len) {
    char leaf[B_PATH]; int dir=open_parent(root,name,true,leaf); if(dir<0) return -1;
    int fd=openat(dir,leaf,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    int e=errno; close(dir); if(fd<0) { errno=e; return -1; }
    const unsigned char *p=data; size_t off=0;
    while(off<len) {
        ssize_t n=write(fd,p+off,len-off);
        if(n<0 && errno==EINTR) continue;
        if(n<=0) { e=errno; close(fd); errno=e; return -1; } off+=(size_t)n;
    } return close(fd);
}
int copy_to(int root,const char *name,const Bytes *b) { return write_at(root,name,b->data,b->len); }

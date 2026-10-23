#include "beryl/beryl.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
int workspace_create(const Config *c,Result *r,int *archive,int *work) {
    int root=open_directory(c->results,true); if(root<0)return -1;
    char resolved[B_PATH];
    if(!realpath(c->results,resolved)) { close(root);return -1; }
    /* Anchor mkdtemp to the verified descriptor, even if a path component is renamed. */
    char template[B_PATH];
    int n=snprintf(template,sizeof template,"/proc/self/fd/%d/case-XXXXXX",root);
    if(n<0 || n>=B_PATH || !mkdtemp(template)){close(root);return -1;}
    const char *leaf=strrchr(template,'/')+1;
    if(path_join(r->directory,resolved,leaf)<0){close(root);return -1;}
    *archive=openat(root,leaf,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);close(root);
    if(*archive<0)return -1;
    if(mkdirat(*archive,"work",0700)<0)return -1;
    *work=openat(*archive,"work",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    return *work<0?-1:0;
}

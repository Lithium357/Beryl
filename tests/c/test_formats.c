#include "beryl/beryl.h"
#include "beryl/copybook.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static void put(const char *path,const char *text){FILE *f=fopen(path,"wb");assert(f);assert(fwrite(text,1,strlen(text),f)==strlen(text));assert(!fclose(f));}
static int invoke(char *const argv[]){pid_t p=fork();assert(p>=0);if(!p){execv(argv[0],argv);_exit(99);}int s;while(waitpid(p,&s,0)<0)assert(errno==EINTR);assert(WIFEXITED(s));return WEXITSTATUS(s);}
int main(int argc,char **argv) {
    assert(argc==2||argc==3);Layout layout;char error[512];
    const char *fixed="000100 01 R.\r\n000200*COMMENT ignored\r\n000300     05 A PIC X(4).\r\n000400/COMMENT ignored\r\n000500     05 B PIC S9(3) COMP-3.\r\n";
    assert(!copybook_parse_format(fixed,strlen(fixed),"fixed",&layout,error));assert(layout.size==6&&layout.count==2);
    const char *bad[]={"000100-    05 X PIC X.\n","000100D    05 X PIC X.\n","ABCDEF 01 R.\n","000100\t01 R.\n"};
    for(size_t i=0;i<sizeof bad/sizeof *bad;i++)assert(copybook_parse_format(bad[i],strlen(bad[i]),"fixed",&layout,error)<0);
    char with_id[100];memset(with_id,' ',72);memcpy(with_id,"000100 01 R. 05 X PIC X.",23);memcpy(with_id+72,"IGNORED\n",8);assert(!copybook_parse_format(with_id,80,"fixed",&layout,error));assert(layout.size==1);
    char cli[B_PATH],root[]="/tmp/beryl-formats-XXXXXX";assert(realpath(argv[1],cli));assert(mkdtemp(root));char source[B_PATH],out[B_PATH];assert(!path_join(source,root,"fixed.cpy"));assert(!path_join(out,root,"generated"));put(source,fixed);
    char *generate[]={cli,"generate","--copybook",source,"--format","fixed","--count","6","--seed","0","--out",out,NULL};assert(!invoke(generate));
    if(argc==3)for(unsigned kind=0;kind<2;kind++) {
        char main_file[B_PATH],fields[B_PATH],resolved[B_PATH],out_root[B_PATH];assert(!path_join(main_file,root,kind?"fixed-copy.cpy":"free-copy.cpy"));assert(!path_join(fields,root,kind?"fixed-fields.cpy":"free-fields.cpy"));assert(!path_join(out_root,root,kind?"resolved-fixed":"resolved-free"));assert(!path_join(resolved,out_root,"resolved.cpy"));
        put(main_file,kind?"000100 01 R.\n000200     COPY \"fixed-fields.cpy\".\n":"01 R.\nCOPY \"free-fields.cpy\".\n");
        put(fields,kind?"000100     05 A PIC 9(3).\n":"05 A PIC 9(3).\n");
        char *preprocess[]={cli,"preprocess","--copybook",main_file,"--format",kind?"fixed":"free","--cobc",argv[2],"--out",out_root,NULL};assert(!invoke(preprocess));
        Bytes b={0};assert(!read_bytes(resolved,B_ENV_LIMIT,&b));assert(!copybook_parse((char *)b.data,b.len,&layout,error));assert(layout.size==3);bytes_free(&b);
        assert(invoke(preprocess)==2);
    }
    printf("formats: fixed columns/comments/CRLF/identification area, rejected indicators, generation%s passed\nevidence: %s\n",argc==3?", real free/fixed COPY preprocessing":"",root);return 0;
}

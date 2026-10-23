#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
int main(int argc,char **argv) {
    unsigned char mode=0;FILE *in=fopen("INPUT.DAT","rb");if(!in)return 90;
    if(fread(&mode,1,1,in)!=1){fclose(in);return 91;}
    if(mode=='M'){fclose(in);return 0;}
    if(mode=='C'){fclose(in);raise(SIGSEGV);return 92;}
    if(mode=='A'){fclose(in);abort();}
    if(mode=='T') {
        fclose(in);pid_t p=fork();if(p<0)return 93;
        if(!p){struct timespec t={1,0};nanosleep(&t,NULL);FILE *f=fopen("descendant-survived","wb");if(f)fclose(f);_exit(0);}
        for(;;)pause();
    }
    if(mode=='L'){fclose(in);for(;;){} }
    if(mode=='S'){fclose(in);return symlink("../input.bin","OUTPUT.DAT")?94:0;}
    if(mode=='D'){fclose(in);if(rmdir("nested"))return 94;return symlink("..","nested")?94:0;}
    if(mode=='P'){fclose(in);if(!fork()){for(;;)pause();}return 0;}
    FILE *out=fopen("OUTPUT.DAT","wb");if(!out){fclose(in);return 95;}
    if(mode=='O' || mode=='F') {
        for(unsigned i=0;i<100000;i++){if(fputc('x',out)==EOF)break;}
        if(mode=='O')for(unsigned i=0;i<100000;i++){fputc('y',stdout);fputc('z',stderr);}
    } else if(mode=='E') {
        const char *v=getenv("BERYL_FIXTURE_VALUE");if(v)fwrite(v,1,strlen(v),out);
        if(argc>1)fwrite(argv[1],1,strlen(argv[1]),out);
    } else {
        unsigned char data[1024];size_t n;
        while((n=fread(data,1,sizeof data,in))>0)if(fwrite(data,1,n,out)!=n)return 96;
    }
    fclose(in);if(fclose(out))return 97;
    return mode=='N'?7:0;
}

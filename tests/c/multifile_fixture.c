#include <stdio.h>
static int copy(const char *input,const char *output){FILE *in=fopen(input,"rb");if(!in)return 10;FILE *out=fopen(output,"wb");if(!out){fclose(in);return 11;}unsigned char data[1024];size_t n;while((n=fread(data,1,sizeof data,in)))if(fwrite(data,1,n,out)!=n)return 12;fclose(in);return fclose(out)?13:0;}
int main(void){int rc=copy("INPUT.DAT","OUTPUT.DAT");return rc?rc:copy("CONFIG.DAT","nested/EXTRA.DAT");}

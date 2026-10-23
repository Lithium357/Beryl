#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef DIFF_BUG
#define DIFF_BUG 0
#endif
int main(void) {
    unsigned char data[1024]; FILE *f=fopen("INPUT.DAT","rb");if(!f)return 90;
    size_t n=fread(data,1,sizeof data,f);fclose(f);if(!n)return 91;
    if(DIFF_BUG && data[0]=='C')abort();
    if(DIFF_BUG && data[0]=='T')for(;;){}
    if(DIFF_BUG && data[0]=='M')return 0;
    f=fopen("OUTPUT.DAT","wb");if(!f)return 92;
    if(data[0]=='L')fputs(DIFF_BUG?"line\n":"line\r\n",f);
    else { if(DIFF_BUG && data[0]=='D')data[n-1]^=1;fwrite(data,1,n,f); }
    fclose(f);
    if(data[0]=='S')fputs(DIFF_BUG?"candidate":"reference",stdout);
    if(data[0]=='E')return DIFF_BUG?7:0;
    if(data[0]=='N')return 7;
    return 0;
}

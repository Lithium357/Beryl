#include "beryl/beryl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void) {
    char h[65];hash_bytes("",0,h);assert(!strcmp(h,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    hash_bytes("abc",3,h);assert(!strcmp(h,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    char *million=malloc(1000000);assert(million);memset(million,'a',1000000);hash_bytes(million,1000000,h);free(million);
    assert(!strcmp(h,"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
    const char *bad[]={"","/tmp/a","../a","a/../b","a//b","a/","./a","a\\b","a\nb","a/./b","C:x"};
    for(size_t i=0;i<sizeof bad/sizeof *bad;i++)assert(!safe_name(bad[i]));
    assert(safe_name("a/b.dat"));
    char dir[]="/tmp/beryl-core-XXXXXX";assert(mkdtemp(dir));int root=open_directory(dir,false);assert(root>=0);
    unsigned char raw[]={0,255,0,1};assert(!write_at(root,"nested/data",raw,sizeof raw));
    char path[B_PATH];assert(!path_join(path,dir,"nested/data"));assert(!hash_file(path,h));char expected[65];hash_bytes(raw,sizeof raw,expected);assert(!strcmp(h,expected));
    Bytes b={0};assert(!read_at(root,"nested/data",2,&b));assert(b.len==2&&b.truncated&&!memcmp(b.data,raw,2));bytes_free(&b);
    assert(write_at(root,"nested/data",raw,4)<0);assert(write_at(root,"../escape",raw,4)<0);
    assert(!path_join(path,dir,"link"));assert(!symlink("/tmp",path));assert(write_at(root,"link/escape",raw,4)<0);assert(open_directory(path,false)<0);
    close(root);puts("core: SHA-256 vectors, safe paths, binary IO, truncation, exclusive writes and symlink refusal passed");return 0;
}

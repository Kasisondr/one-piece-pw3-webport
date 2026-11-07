#include "host_fixture.h"
#include <string.h>
void vita_hle_sceIoOpen(void);void vita_hle_sceIoClose(void);
void vita_hle_sceIoLseek32(void);void vita_hle_sceIoRead(void);
void vita_hle_sceIoPread(void);void vita_hle_sceIoPwrite(void);
int main(void){
    const char *file="positioned-io-fixture.bin";
    FILE *f=fopen(file,"wb");assert(f);assert(fwrite("0123456789",1,10,f)==10);fclose(f);
    assert(!vita_mem_init(0x81000000,0x10000));sp=0x8100F000;
    vita_io_mount("app0",".");const char *path="app0:positioned-io-fixture.bin";
    memcpy(vita_mem_ptr(0x81000100,64),path,strlen(path)+1);
    r0=0x81000100;r1=3;r2=0;vita_hle_sceIoOpen();uint32_t fd=r0;assert(fd<0x80000000);
    r0=fd;r1=2;r2=0;vita_hle_sceIoLseek32();assert(r0==2);
    vita_write32(sp,5);vita_write32(sp+4,0);
    r0=fd;r1=0x81000200;r2=3;r3=0xDEADBEEF;vita_hle_sceIoPread();assert(r0==3);
    assert(!memcmp(vita_mem_ptr(0x81000200,3),"567",3));
    r0=fd;r1=0;r2=1;vita_hle_sceIoLseek32();assert(r0==2);
    vita_write32(sp,5);vita_write32(sp+4,1);r0=fd;r1=0x81000200;r2=3;vita_hle_sceIoPread();assert(!r0);
    r0=fd;r1=0;r2=1;vita_hle_sceIoLseek32();assert(r0==2);
    vita_write8(0x81000200,'X');vita_write32(sp,8);vita_write32(sp+4,0);
    r0=fd;r1=0x81000200;r2=1;vita_hle_sceIoPwrite();assert(r0==1);
    r0=fd;r1=0;r2=1;vita_hle_sceIoLseek32();assert(r0==2);
    vita_write32(sp+4,0x80000000);r0=fd;r1=0x81000200;r2=1;vita_hle_sceIoPread();assert(r0==0x80010016);
    r0=fd;vita_hle_sceIoClose();assert(!r0);
    f=fopen(file,"rb");assert(f);char bytes[10];assert(fread(bytes,1,10,f)==10);fclose(f);assert(!memcmp(bytes,"01234567X9",10));assert(!remove(file));
    puts("positioned I/O: stack ABI, 64-bit offsets, EOF, read/write and unchanged cursor passed");return 0;
}

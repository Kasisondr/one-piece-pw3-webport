#include "host_fixture.h"
#include <string.h>
void vita_hle_sceIoOpen(void);void vita_hle_sceIoClose(void);
void vita_hle_sceIoLseek(void);void vita_hle_sceIoRead(void);void vita_hle_sceIoGetstatByFd(void);
void vita_hle_sceIoPread(void);
static uint32_t fd;
#define BUFFER 0x81001000u
static void seek_to(uint64_t n) {
    vita_write32(sp,0);r0=fd;r2=(uint32_t)n;r3=(uint32_t)(n>>32);vita_hle_sceIoLseek();
    assert(r0==(uint32_t)n && r1==(uint32_t)(n>>32));
}
static uint32_t read_hash(uint32_t size) {
    r0=fd;r1=BUFFER;r2=size;vita_hle_sceIoRead();assert(r0==size);
    uint32_t hash=2166136261u;
    for(uint32_t i=0;i<size;i++)hash=(hash^vita_read8(BUFFER+i))*16777619u;
    return hash;
}
int main(void) {
    assert(!vita_mem_init(0x81000000,0x10000));sp=0x8100F000;
    vita_io_mount("app0","/app0");
    const char *path="app0:/LINKDATA_NGP/LINKDATA_NA.C";
    memcpy(vita_mem_ptr(0x81000100,strlen(path)+1),path,strlen(path)+1);
    r0=0x81000100;r1=1;r2=0;vita_hle_sceIoOpen();fd=r0;assert(fd>=0x40200000 && fd<0x40200020);
    r0=fd;r1=0x81000200;vita_hle_sceIoGetstatByFd();assert(!r0);
    uint64_t length=(uint64_t)vita_read32(0x81000208)|((uint64_t)vita_read32(0x8100020C)<<32);
    assert(length>262180);
    uint32_t first=read_hash(32);seek_to(262140);uint32_t cross=read_hash(32);
    seek_to(length-17);uint32_t last=read_hash(17);
    r0=fd;r1=BUFFER;r2=1;vita_hle_sceIoRead();assert(!r0);
    seek_to(0x10000000AuLL);r0=fd;r1=BUFFER;r2=1;vita_hle_sceIoRead();assert(!r0);
    seek_to(2);vita_write32(sp,262140);vita_write32(sp+4,0);
    r0=fd;r1=BUFFER;r2=32;vita_hle_sceIoPread();assert(r0==32);
    vita_write32(sp,1);r0=fd;r2=r3=0;vita_hle_sceIoLseek();assert(r0==2 && r1==0);
    r0=fd;vita_hle_sceIoClose();assert(!r0 && !vita_mem_bad_access);
    printf("WEB_IO_OK %llu %08X %08X %08X\n",(unsigned long long)length,first,cross,last);
    return 0;
}

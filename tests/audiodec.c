#include "host_fixture.h"
/* Reuse upstream's synthetic encoder and independent transform checks. */
#define main upstream_atrac9_tests
#include "../external/vitarecomp/tests/test_atrac9.c"
#undef main
void vita_hle_sceAudiodecInitLibrary(void);void vita_hle_sceAudiodecTermLibrary(void);
void vita_hle_sceAudiodecCreateDecoder(void);void vita_hle_sceAudiodecDeleteDecoder(void);
void vita_hle_sceAudiodecDecode(void);void vita_hle_sceAudiodecClearContext(void);
void vita_hle_sceAudiodecGetInternalError(void);
#define CTRL 0x81000100u
#define INFO 0x81000200u
#define INIT 0x81000300u
#define ES   0x81001000u
#define PCM  0x81003000u
int main(void) {
    upstream_atrac9_tests();
    assert(!vita_mem_init(0x81000000,0x10000));
    r0=0x1003;r1=0;vita_hle_sceAudiodecInitLibrary();assert(r0==0x807F0008);
    vita_write32(INIT,8);vita_write32(INIT+4,2);
    r0=0x1003;r1=INIT;vita_hle_sceAudiodecInitLibrary();assert(!r0);
    r0=0x1003;r1=INIT;vita_hle_sceAudiodecInitLibrary();assert(r0==0x807F0003);
    r0=0x1004;r1=INIT;vita_hle_sceAudiodecInitLibrary();assert(r0==0x807F000E);
    for(int log2=0;log2<=2;log2++) {
        const int count=1<<log2;
        uint8_t config[4];configuration(config,7,0,128,log2);
        bw stream={0};block b={0};
        b.channels=1;b.units=10;b.shared_from=10;b.gradient=8;b.way[0]=PLAIN;
        for(int u=0;u<10;u++)b.sf[0][u]=18;
        b.coarse[0][0]=1;b.coarse[0][1]=-1;
        uint32_t end[8];
        for(int i=0;i<count;i++){put_block(&stream,&b,NULL,0,i==0);end[i]=(uint32_t)(stream.at/8);}
        assert(end[count-1]<(uint32_t)(128*count));
        atrac9_decoder reference;float decoded[256*8];int16_t expected[256*8];
        assert(atrac9_open(&reference,config));assert(atrac9_decode(&reference,stream.buf,decoded));
        atrac9_pcm16(decoded,expected,256*count);
        vita_write32(CTRL,40);vita_write32(CTRL+32,16);vita_write32(CTRL+36,INFO);
        vita_write32(INFO,28);memcpy(vita_mem_ptr(INFO+4,4),config,4);
        r0=CTRL;r1=0x1003;vita_hle_sceAudiodecCreateDecoder();assert(!r0);
        assert(vita_read32(INFO+8)==1 && vita_read32(INFO+16)==48000);
        assert(vita_read32(INFO+20)==128*(uint32_t)count && vita_read32(INFO+24)==(uint32_t)count);
        assert(vita_read32(CTRL+28)==512);
        r0=0x1003;vita_hle_sceAudiodecTermLibrary();assert(r0==0x807F0006);
        memcpy(vita_mem_ptr(ES,128*count),stream.buf,128*count);
        uint32_t offset=0;
        for(int i=0;i<count;i++) {
            vita_write32(CTRL+8,ES+offset);vita_write32(CTRL+20,PCM);
            r0=CTRL;vita_hle_sceAudiodecDecode();assert(!r0);
            uint32_t used=vita_read32(CTRL+12);
            assert(used==(i==count-1?128*count-offset:end[i]-offset));offset+=used;
            assert(vita_read32(CTRL+24)==512);
            assert(!memcmp(vita_mem_ptr(PCM,512),expected+256*i,512));
        }
        assert(offset==128*(uint32_t)count);
        r0=CTRL;vita_hle_sceAudiodecClearContext();assert(!r0);
        vita_write32(CTRL+8,ES);vita_write32(CTRL+28,1);
        r0=CTRL;vita_hle_sceAudiodecDecode();assert(r0==0x807F000D && !vita_read32(CTRL+24));
        vita_write32(CTRL+28,512);vita_write8(ES,0xFF);
        r0=CTRL;vita_hle_sceAudiodecDecode();assert(r0==0x807F0000);
        r0=CTRL;r1=INIT;vita_hle_sceAudiodecGetInternalError();assert(!r0 && vita_read32(INIT)==0x807F0000);
        memcpy(vita_mem_ptr(ES,128*count),stream.buf,128*count);
        r0=CTRL;vita_hle_sceAudiodecClearContext();assert(!r0);
        r0=CTRL;vita_hle_sceAudiodecDecode();assert(!r0);
        assert(!memcmp(vita_mem_ptr(PCM,512),expected,512));
        r0=CTRL;vita_hle_sceAudiodecDeleteDecoder();assert(!r0);
        r0=CTRL;vita_hle_sceAudiodecDecode();assert(r0==0x807F0009);
    }
    r0=0x1003;vita_hle_sceAudiodecTermLibrary();assert(!r0);
    puts("audiodec: synthetic AT9, frame consumption, PCM, lifecycle, reset and errors passed");return 0;
}

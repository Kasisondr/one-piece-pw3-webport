/* SceAudiodec AT9: real decoding through the pinned MIT ATRAC9 decoder.
 * Guest ABI: vita-headers/include/psp2/audiodec.h. Other codecs return an
 * explicit unsupported error; they are not replaced with silent output. */
#include "vitarecomp/recomp_rt.h"
#include "atrac9.h"
#include <string.h>
#define AE(x) (0x807F0000u + (x))
#define AT9 0x1003u
extern int port_atrac9_frame(atrac9_decoder*,const uint8_t*,uint32_t,uint32_t,float*,uint32_t*);
typedef struct {
    uint32_t handle, frame, used, error;
    atrac9_decoder decoder;
} audio_decoder;
static audio_decoder decoders[16];
static uint32_t limit, channels, next_handle = 1;
static uint32_t ctrl_error(uint32_t p) {
    if (!vita_mem_ptr(p,40) || !p) return AE(8);
    return vita_read32(p)==40 ? 0 : AE(13);
}
static audio_decoder *lookup(uint32_t p) {
    uint32_t h=vita_read32(p+4);
    for(unsigned i=0;i<16;i++)if(h && decoders[i].handle==h)return &decoders[i];
    return NULL;
}
void vita_hle_sceAudiodecInitLibrary(void) {
    uint32_t type=r0,p=r1;
    if(type!=AT9){r0=type>=0x1004 && type<=0x1006?AE(14):AE(1);return;}
    if(limit){r0=AE(3);return;}
    if(!p || !vita_mem_ptr(p,8)){r0=AE(8);return;}
    uint32_t n=vita_read32(p+4);
    if(vita_read32(p)!=8 || !n || n>16){r0=AE(2);return;}
    limit=n;r0=0;
}
void vita_hle_sceAudiodecTermLibrary(void) {
    if(r0!=AT9){r0=AE(1);return;}
    if(!limit){r0=AE(5);return;}
    if(channels){r0=AE(6);return;}
    limit=0;r0=0;
}
void vita_hle_sceAudiodecCreateDecoder(void) {
    uint32_t p=r0,type=r1,e=ctrl_error(p);
    if(e){r0=e;return;}
    if(type!=AT9){r0=type>=0x1004 && type<=0x1006?AE(14):AE(1);return;}
    if(!limit){r0=AE(5);return;}
    if(lookup(p)){r0=AE(6);return;}
    if(vita_read32(p+32)!=16){r0=AE(12);return;}
    uint32_t info=vita_read32(p+36);
    if(!info || !vita_mem_ptr(info,28)){r0=AE(8);return;}
    if(vita_read32(info)!=28){r0=AE(13);return;}
    const uint8_t *config=vita_mem_ptr(info+4,4);
    atrac9_info format;
    if(!atrac9_config(config,&format) || format.channels>2){r0=0x807F2000u;return;}
    if(channels+format.channels>limit){r0=AE(11);return;}
    audio_decoder *d=NULL;
    for(unsigned i=0;i<16;i++)if(!decoders[i].handle){d=&decoders[i];break;}
    if(!d){r0=AE(7);return;}
    memset(d,0,sizeof *d);
    if(!atrac9_open(&d->decoder,config)){r0=0x807F2000u;return;}
    d->handle=next_handle++;if(!next_handle)next_handle=1;
    channels+=format.channels;
    vita_write32(p+4,d->handle);vita_write32(p+12,0);vita_write32(p+24,0);
    vita_write32(p+16,format.superframe_bytes<1024?format.superframe_bytes:1024);
    vita_write32(p+28,format.frame_samples*format.channels*2);
    vita_write32(info+8,format.channels);
    vita_write32(info+12,(uint32_t)((uint64_t)format.superframe_bytes*8*format.rate/format.superframe_samples));
    vita_write32(info+16,format.rate);vita_write32(info+20,format.superframe_bytes);
    vita_write32(info+24,format.frames);r0=0;
}
void vita_hle_sceAudiodecDeleteDecoder(void) {
    uint32_t p=r0,e=ctrl_error(p);if(e){r0=e;return;}
    audio_decoder *d=lookup(p);if(!d){r0=AE(9);return;}
    channels-=d->decoder.info.channels;memset(d,0,sizeof *d);
    vita_write32(p+4,0);r0=0;
}
void vita_hle_sceAudiodecClearContext(void) {
    uint32_t p=r0,e=ctrl_error(p);if(e){r0=e;return;}
    audio_decoder *d=lookup(p);if(!d){r0=AE(9);return;}
    atrac9_reset(&d->decoder);d->frame=d->used=d->error=0;
    vita_write32(p+12,0);vita_write32(p+24,0);r0=0;
}
void vita_hle_sceAudiodecGetInternalError(void) {
    uint32_t p=r0,out=r1,e=ctrl_error(p);if(e){r0=e;return;}
    audio_decoder *d=lookup(p);if(!d){r0=AE(9);return;}
    if(!out || !vita_mem_ptr(out,4)){r0=AE(8);return;}
    vita_write32(out,d->error);r0=0;
}
void vita_hle_sceAudiodecDecode(void) {
    uint32_t p=r0,e=ctrl_error(p);if(e){r0=e;return;}
    audio_decoder *d=lookup(p);if(!d){r0=AE(9);return;}
    vita_write32(p+12,0);vita_write32(p+24,0);
    if(vita_read32(p+32)!=16){r0=AE(12);return;}
    atrac9_info *f=&d->decoder.info;
    uint32_t remaining=f->superframe_bytes-d->used;
    uint32_t bytes=vita_read32(p+16),pcm_bytes=f->frame_samples*f->channels*2;
    if(bytes>remaining)bytes=remaining;
    if(!bytes || vita_read32(p+28)<pcm_bytes){r0=AE(13);return;}
    uint32_t input=vita_read32(p+8),output=vita_read32(p+20);
    const uint8_t *in=vita_mem_ptr(input,bytes);
    int16_t *out=vita_mem_ptr(output,pcm_bytes);
    if(!input || !output || !in || !out){r0=AE(8);return;}
    float samples[ATRAC9_FRAME_MOST*2];uint32_t used;
    if(!port_atrac9_frame(&d->decoder,in,bytes,d->frame==0,samples,&used)){
        d->error=AE(0);d->frame=d->used=0;r0=AE(0);return;
    }
    if(d->frame+1==f->frames) {
        if(bytes<remaining){atrac9_reset(&d->decoder);d->error=AE(13);d->frame=d->used=0;r0=AE(13);return;}
        used=remaining;d->frame=d->used=0;
    } else {d->frame++;d->used+=used;}
    atrac9_pcm16(samples,out,f->frame_samples*f->channels);
    d->error=0;vita_write32(p+12,used);vita_write32(p+24,pcm_bytes);r0=0;
}

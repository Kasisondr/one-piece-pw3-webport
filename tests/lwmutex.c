#include "vitarecomp/recomp_rt.h"
#include "vitarecomp/host.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
void vita_host_vblank(void){}
uint32_t vita_host_buttons(void){return 0;}
void vita_host_sticks(uint8_t xy[4]){for(int i=0;i<4;i++)xy[i]=128;}
int vita_host_touch(int *x,int *y){(void)x;(void)y;return 0;}
void vita_host_exit(int status){exit(status);}
const vita_var_import vita_var_imports[1]={{0}};
const uint32_t vita_var_import_count=0;
void vita_hle_sceKernelCreateLwMutex(void);
void vita_hle_sceKernelGetLwMutexInfo(void);
void vita_hle_sceKernelLockLwMutex(void);
void vita_hle_sceKernelUnlockLwMutex(void);
void vita_hle_sceKernelDeleteLwMutex(void);
void vita_hle_sceKernelCreateEventFlag(void);
void vita_hle_sceKernelPollEventFlag(void);
void vita_hle_sceKernelDeleteEventFlag(void);
void vita_hle_sceGxmProgramParameterIsRegFormat(void);
void vita_hle_sceGxmProgramParameterGetIndex(void);
void vita_hle_sceDisplayGetRefreshRate(void);
void vita_hle_sceClibMemcpy_safe(void);

static void test_event_flag_capacity(void) {
    // The game's effects initialization keeps more than 64 flags live.
    uint32_t flags[80];
    for (unsigned i = 0; i < 80; i++) {
        r0 = 0;
        r1 = 0;
        r2 = 1u << (i % 32);
        r3 = 0;
        vita_hle_sceKernelCreateEventFlag();
        assert((int32_t)r0 > 0);
        for (unsigned j = 0; j < i; j++) assert(flags[j] != r0);
        flags[i] = r0;
    }
    for (unsigned i = 0; i < 80; i++) {
        r0 = flags[i];
        r1 = 1u << (i % 32);
        r2 = 0;
        r3 = 0;
        vita_hle_sceKernelPollEventFlag();
        assert(r0 == 0);
        r0 = flags[i];
        vita_hle_sceKernelDeleteEventFlag();
        assert(r0 == 0);
    }
}

int main(void){
    const uint32_t work=0x81000100u,info=0x81000200u;
    assert(!vita_mem_init(0x81000000u,0x10000));
    r0=work;r1=info;vita_hle_sceKernelGetLwMutexInfo();assert(r0==0x80028181u);
    r0=work;r1=0;r2=2;r3=1;vita_hle_sceKernelCreateLwMutex();assert(r0==0);
    assert(vita_read32(work)!=0);assert(vita_read32(work+8)==1);assert(vita_read32(work+12)==2);assert(vita_read32(work+16)!=0);
    r0=work;r1=1;r2=0;vita_hle_sceKernelLockLwMutex();assert(!r0&&vita_read32(work+8)==2);
    vita_write32(info,64);r0=work;r1=info;vita_hle_sceKernelGetLwMutexInfo();assert(!r0);
    assert(vita_read32(info)==64&&vita_read32(info+4)==vita_read32(work+16));
    assert(vita_read32(info+44)==work&&vita_read32(info+48)==1&&vita_read32(info+52)==2);
    r0=work;r1=3;vita_hle_sceKernelUnlockLwMutex();assert(r0==0x80028184u&&vita_read32(work+8)==2);
    r0=work;r1=2;vita_hle_sceKernelUnlockLwMutex();assert(!r0&&vita_read32(work)==0&&vita_read32(work+8)==0);
    // A size-limited info request cannot overwrite its caller's following bytes.
    vita_write32(info,4);vita_write32(info+4,0x12345678);r0=work;r1=info;vita_hle_sceKernelGetLwMutexInfo();assert(!r0&&vita_read32(info+4)==0x12345678);
    r0=work;vita_hle_sceKernelDeleteLwMutex();assert(!r0);
    r0=work;r1=info;vita_hle_sceKernelGetLwMutexInfo();assert(r0==0x80028181u);
    r0=work;r1=0;r2=0;r3=0;vita_hle_sceKernelCreateLwMutex();assert(!r0);
    r0=work;r1=1;r2=0;vita_hle_sceKernelLockLwMutex();assert(!r0);
    r0=work;r1=1;r2=0;vita_hle_sceKernelLockLwMutex();assert(r0==0x80028182u);
    const uint32_t gxp=0x81001000u,param=0x81001200u,vary=0x81001100u;
    vita_write32(gxp+0x2C,vary-gxp-0x2C);vita_write32(vary+8,0x80000000u);vita_write32(vary+12,0x80000001u);
    const uint32_t regs[]={0,30,31,32,33,63,64};
    const uint32_t expected[]={0,0,1,1,0,1,0};
    for(unsigned i=0;i<7;i++){vita_write32(param+12,regs[i]);r0=gxp;r1=param;vita_hle_sceGxmProgramParameterIsRegFormat();assert(r0==expected[i]);}
    vita_write32(gxp+0x14,1);r0=gxp;r1=param;vita_hle_sceGxmProgramParameterIsRegFormat();assert(!r0);
    vita_write32(gxp+0x14,0);vita_write32(param+12,31);vita_write8(param+4,1);r0=gxp;r1=param;vita_hle_sceGxmProgramParameterIsRegFormat();assert(!r0);
    vita_write8(param+4,0);vita_write32(gxp+0x2C,0xFFFFFFFFu);r0=gxp;r1=param;vita_hle_sceGxmProgramParameterIsRegFormat();assert(!r0);
    vita_write32(gxp+0x28,param-gxp-0x28);vita_write32(gxp+0x24,3);
    r0=gxp;r1=param+32;vita_hle_sceGxmProgramParameterGetIndex();assert(r0==2);
    r0=gxp;r1=param+48;vita_hle_sceGxmProgramParameterGetIndex();assert(r0==0);
    r0=gxp;r1=param+17;vita_hle_sceGxmProgramParameterGetIndex();assert(r0==0);
    r0=info;vita_hle_sceDisplayGetRefreshRate();assert(r0==0&&vita_read32(info)==0x42700000u);
    vita_write32(info,0x12345678);r0=info+4;r1=info;r2=4;vita_hle_sceClibMemcpy_safe();assert(r0==info+4 && vita_read32(info+4)==0x12345678);
    r0=r1=r2=0;vita_hle_sceClibMemcpy_safe();assert(!r0);
    test_event_flag_capacity();
    puts("Kernel checks passed, including 80 concurrent event flags");return 0;
}

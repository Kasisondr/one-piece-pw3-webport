#include "host_fixture.h"
#include "vitarecomp/dispatch.h"
#include "sched.h"
int port_thread_exit_status(uint32_t uid,uint32_t out);
void vita_hle__sceUltUlthreadRuntimeOptParamInitialize(void);
void vita_hle_sceUltUlthreadRuntimeGetWorkAreaSize(void);
void vita_hle__sceUltUlthreadRuntimeCreate(void);
void vita_hle__sceUltUlthreadCreate(void);
void vita_hle_sceUltUlthreadTryJoin(void);
void vita_hle_sceUltUlthreadJoin(void);
void vita_hle_sceUltUlthreadRuntimeDestroy(void);
#define BASE 0x81000000u
#define RT (BASE+0x400)
#define OPT (BASE+0x1000)
#define NAME (BASE+0x1100)
#define WORK (BASE+0x2000)
#define JOB (BASE+0x10000)
#define RESULT (BASE+0x1200)
#define GATE 0x88888888u
static uint32_t gate,entered,callback_sp,published_busy;
static void callback(void){
    uint32_t arg=r0;entered++;callback_sp=sp;
    r4=0xD00DF00Du;vfp_setu(0,0x12345678u);
    while(!gate)vita_sched_block(VW_ULT,GATE,0);
    assert(r4==0xD00DF00Du && vfp_getu(0)==0x12345678u);
    published_busy=0;
    r0=arg+17;
}
static void create_job(uint32_t object,uint32_t entry,uint32_t arg,uint32_t stack,uint32_t size){
    vita_write32(sp,stack);vita_write32(sp+4,size);vita_write32(sp+8,RT);vita_write32(sp+12,0);vita_write32(sp+16,0x03300000u);
    r0=object;r1=NAME;r2=entry;r3=arg;vita_hle__sceUltUlthreadCreate();
}
int main(void){
    assert(!vita_mem_init(BASE,0x400000));
    vita_memblock_init(BASE+0x200000,0x100000);vita_kernel_init(BASE+0x100000,0x10000);
    vita_main_stack_init(BASE+0x300000,0x100000);sp=BASE+0x3FFF00;vita_clock_virtual();
    vita_write32(NAME,0x74736574);vita_write32(NAME+4,0);
    r0=0;r1=0x3300000;vita_hle__sceUltUlthreadRuntimeOptParamInitialize();assert(r0==0x80810001u);
    r0=OPT+1;vita_hle__sceUltUlthreadRuntimeOptParamInitialize();assert(r0==0x80810002u);
    r0=OPT;r1=0x3300000;vita_hle__sceUltUlthreadRuntimeOptParamInitialize();assert(!r0);
    assert(vita_read32(OPT)==0x6C696275u && vita_read32(OPT+12)==0x3300000);
    assert(vita_read32(OPT+16)==4096 && vita_read32(OPT+20)==0x10000100u && vita_read32(OPT+24)==0x70000);
    for(unsigned i=28;i<128;i++)assert(!vita_read8(OPT+i));
    r0=32;r1=3;vita_hle_sceUltUlthreadRuntimeGetWorkAreaSize();assert(r0==3728);
    vita_write32(OPT+16,0x4000);
    vita_write32(sp,WORK);vita_write32(sp+4,OPT);vita_write32(sp+8,0x3300000);
    r0=RT;r1=NAME;r2=2;r3=1;vita_hle__sceUltUlthreadRuntimeCreate();assert(!r0);
    uint32_t entry=vita_dispatch_host(callback);r4=0xCAFEBABE;vfp_setu(0,0xFACE1234);
    uint32_t dormant=vita_thread_create("dormant",entry,0xA0,0x1000);
    assert(port_thread_exit_status(dormant,RESULT)==(int)0x80028027u);
    assert(port_thread_exit_status(0,RESULT)==(int)0x80028028u);
    assert(port_thread_exit_status(dormant,0)==(int)0x80020006u);
    assert(!vita_thread_delete(dormant));
    assert(port_thread_exit_status(dormant,RESULT)==(int)0x80028021u);
    create_job(JOB,entry,42,0,0);assert(!r0 && entered==0);assert(r4==0xCAFEBABE&&vfp_getu(0)==0xFACE1234);
    vita_sched_yield();assert(entered==1);
    r0=JOB;r1=RESULT;vita_hle_sceUltUlthreadTryJoin();assert(r0==0x80810008u);
    r0=RT;vita_hle_sceUltUlthreadRuntimeDestroy();assert(r0==0x80810007u);
    create_job(JOB+256,entry,43,BASE+0x20000,0x2000);assert(!r0&&entered==1);
    vita_sched_yield();assert(entered==2);assert(callback_sp>=BASE+0x20000&&callback_sp<BASE+0x22000);
    create_job(JOB+512,entry,0,0,0);assert(r0==0x80810008u);
    gate=1;vita_sched_wake(VW_ULT,GATE,1);
    r0=JOB;r1=RESULT;vita_hle_sceUltUlthreadJoin();assert(!r0&&vita_read32(RESULT)==59);
    r0=JOB+256;r1=RESULT;vita_hle_sceUltUlthreadJoin();assert(!r0&&vita_read32(RESULT)==60);
    // Repeated submissions reuse the same reserved guest stack and thread slot.
    uint32_t stack_seen=0;
    for(unsigned i=0;i<200;i++){
        create_job(JOB,entry,i,0,0);assert(!r0);
        published_busy=1; /* The game publishes this immediately after Create. */
        r0=JOB;r1=RESULT;vita_hle_sceUltUlthreadTryJoin();assert(r0==0x80810008u);
        vita_sched_yield();
        assert(!published_busy);
        if(i)assert(callback_sp==stack_seen);else stack_seen=callback_sp;
        r0=JOB;r1=RESULT;vita_hle_sceUltUlthreadTryJoin();assert(!r0&&vita_read32(RESULT)==i+17);
    }
    r0=RT;vita_hle_sceUltUlthreadRuntimeDestroy();assert(!r0);
    puts("ULT: options, lifecycle, real callbacks, blocking, join, context preservation, supplied stacks and reuse passed");
    return 0;
}

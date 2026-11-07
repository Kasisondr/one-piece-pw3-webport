/* Bring-up backend for the seven ULT runtime/job imports used by PCSE00638.
 * ABI facts were measured from its own decrypted libult, see docs/ULT.md.
 * Each logical ULT executes as a context on vitarecomp's serialized guest
 * scheduler. This is not an implementation of the SceFiber export surface;
 * kernel thread identity / worker affinity equivalence is not claimed.
 */
#include "vitarecomp/recomp_rt.h"
#include "vitarecomp/hle.h"
#include "vitarecomp/dispatch.h"
#include "sched.h"
#include "hle_sysmem.h"
#include "ult_runtime.h"
#include <string.h>
#include <limits.h>

#define U_NULL  0x80810001u
#define U_ALIGN 0x80810002u
#define U_RANGE 0x80810003u
#define U_VALUE 0x80810004u
#define U_STATE 0x80810006u
#define U_BUSY  0x80810007u
#define U_AGAIN 0x80810008u
#define RUNTIME_BYTES 1024u
#define THREAD_BYTES 256u
#define OPTION_BYTES 128u
#define MAX_RUNTIMES 8
#define MAX_JOBS 64
typedef struct {uint32_t object,limit,workers,stack_size,priority,active,version;} ult_runtime;
typedef struct {uint32_t object,uid,runtime,entry,arg,result,done,joining,stack,size;} ult_job;
static ult_runtime runtimes[MAX_RUNTIMES];
static ult_job jobs[MAX_JOBS];

static uint32_t check_object(uint32_t p,uint32_t n){
    if(!p || !vita_mem_ptr(p,n))return U_NULL;
    return p&7u?U_ALIGN:0;
}
static void option_header(uint32_t p,uint32_t version){
    vita_write32(p,0x6C696275u);vita_write32(p+4,0x6C746D61u);
    vita_write32(p+8,0x67696300u);vita_write32(p+12,version);
}
static int option_valid(uint32_t p){
    return vita_mem_ptr(p,OPTION_BYTES) && !(p&7u) &&
        vita_read32(p)==0x6C696275u && vita_read32(p+4)==0x6C746D61u &&
        vita_read32(p+8)==0x67696300u;
}
static ult_runtime *runtime_at(uint32_t p){
    for(unsigned i=0;i<MAX_RUNTIMES;i++)if(runtimes[i].object==p && p)return &runtimes[i];
    return NULL;
}
static ult_job *job_at(uint32_t p){
    for(unsigned i=0;i<MAX_JOBS;i++)if(jobs[i].object==p && p)return &jobs[i];
    return NULL;
}
static void guest_name(char out[32],uint32_t p){
    memset(out,0,32);
    for(unsigned i=0;i<31;i++){if(!vita_mem_ptr(p+i,1))break;out[i]=(char)vita_read8(p+i);if(!out[i])break;}
}
static uint32_t work_bytes(uint32_t n,uint32_t w){
    uint64_t total=((32ull*w+135)&~7ull)+256ull*w+
        ((36ull*(n+w)+103)&~7ull)+((36ull*n+103)&~7ull)+4ull*n;
    return total<=INT32_MAX?(uint32_t)total:U_RANGE;
}
void vita_hle__sceUltUlthreadRuntimeOptParamInitialize(void){
    uint32_t p=r0,version=r1,error=check_object(p,OPTION_BYTES);
    if(error){r0=error;return;}
    memset(vita_mem_ptr(p,OPTION_BYTES),0,OPTION_BYTES);option_header(p,version);
    vita_write32(p+16,4096);vita_write32(p+20,0x10000100u);vita_write32(p+24,0x70000u);r0=0;
}
void vita_hle_sceUltUlthreadRuntimeGetWorkAreaSize(void){r0=work_bytes(r0,r1);}
void vita_hle__sceUltUlthreadRuntimeCreate(void){
    uint32_t object=r0,name=r1,limit=r2,workers=r3;
    if(!vita_mem_ptr(sp,12)){r0=U_NULL;return;}
    uint32_t work=vita_read32(sp),opt=vita_read32(sp+4),version=vita_read32(sp+8);
    uint32_t error=check_object(object,RUNTIME_BYTES);
    if(!error)error=check_object(work,8);
    if(error){r0=error;return;}
    if(!name || !vita_mem_ptr(name,1)){r0=U_NULL;return;}
    if(!limit || !workers || limit>MAX_JOBS || workers>64){r0=U_VALUE;return;}
    if(!vita_mem_ptr(work,work_bytes(limit,workers))){r0=U_NULL;return;}
    if(opt && !option_valid(opt)){r0=opt&7?U_ALIGN:U_VALUE;return;}
    if(runtime_at(object)){r0=U_STATE;return;}
    ult_runtime *rt=NULL;
    for(unsigned i=0;i<MAX_RUNTIMES;i++)if(!runtimes[i].object){rt=&runtimes[i];break;}
    if(!rt){r0=U_AGAIN;return;}
    uint32_t stack_size=opt?vita_read32(opt+16):4096;
    if(stack_size && (stack_size<512 || stack_size>0x1000000u || (stack_size&7u))){r0=U_VALUE;return;}
    memset(rt,0,sizeof(*rt));rt->object=object;rt->limit=limit;rt->workers=workers;rt->version=version;
    rt->stack_size=stack_size;rt->priority=opt?vita_read32(opt+20):0x10000100u;
    memset(vita_mem_ptr(object,RUNTIME_BYTES),0,RUNTIME_BYTES);option_header(object,version);
    vita_write32(object+32,workers);vita_write32(object+36,work);vita_write32(object+44,limit);
    vita_write32(object+52,stack_size);
    vita_hle_trace("ULT runtime 0x%08X: %u logical jobs, %u requested workers, stack 0x%X",object,limit,workers,stack_size);
    r0=0;
}
static void job_entry(void){
    uint32_t uid=vita_thread_self();ult_job *j=NULL;
    for(unsigned i=0;i<MAX_JOBS;i++)if(jobs[i].object && jobs[i].uid==uid){j=&jobs[i];break;}
    if(!j){r0=U_STATE;return;}
    uint32_t entry=j->entry;r0=j->arg;
    vita_dispatch(0,entry);
    j->result=r0;j->done=1;vita_write32(j->object+176,r0);vita_write16(j->object+160,2);
    r0=j->result;
}
void vita_hle__sceUltUlthreadCreate(void){
    uint32_t object=r0,name=r1,entry=r2,arg=r3;
    if(!vita_mem_ptr(sp,20)){r0=U_NULL;return;}
    uint32_t stack=vita_read32(sp),size=vita_read32(sp+4),rp=vita_read32(sp+8),opt=vita_read32(sp+12),version=vita_read32(sp+16);
    uint32_t error=check_object(object,THREAD_BYTES);
    if(error){r0=error;return;}
    ult_runtime *rt=runtime_at(rp);
    if(!name || !entry || !rt || !vita_mem_ptr(name,1)){r0=U_NULL;return;}
    if(job_at(object)){r0=U_STATE;return;}
    if(opt && !option_valid(opt)){r0=opt&7?U_ALIGN:U_VALUE;return;}
    if(rt->active>=rt->limit){r0=U_AGAIN;return;}
    if(stack){
        error=check_object(stack,size);
        if(error){r0=error;return;}
        if(size<512 || (size&7u)){r0=U_VALUE;return;}
    }else if(size || !rt->stack_size){r0=U_VALUE;return;}
    ult_job *j=NULL;
    for(unsigned i=0;i<MAX_JOBS;i++)if(!jobs[i].object){j=&jobs[i];break;}
    if(!j){r0=U_AGAIN;return;}
    // Keep owned stacks for reuse instead of leaking one for every submitted job.
    if(!stack){
        size=rt->stack_size;
        if(j->size<size){uint32_t base=vita_sysmem_reserve(size);if(!base){r0=U_AGAIN;return;}j->stack=base;j->size=size;}
        stack=j->stack;
    }
    char text[32];guest_name(text,name);
    uint32_t trampoline=vita_dispatch_host(job_entry);
    if(!trampoline){r0=U_AGAIN;return;}
    uint32_t uid=vita_thread_create(text,trampoline,rt->priority,size);
    if(!uid){r0=U_AGAIN;return;}
    if(port_thread_stack(uid,stack,size)){vita_thread_delete(uid);r0=U_VALUE;return;}
    j->object=object;j->uid=uid;j->runtime=rp;j->entry=entry;j->arg=arg;j->done=0;j->joining=0;j->result=0;rt->active++;
    memset(vita_mem_ptr(object,THREAD_BYTES),0,THREAD_BYTES);option_header(object,version);
    vita_write32(object+168,entry);vita_write32(object+172,arg);vita_write32(object+196,rp);vita_write16(object+160,1);
    int rc=port_thread_start_deferred(uid);
    if(rc){rt->active--;j->object=0;vita_thread_delete(uid);r0=(uint32_t)rc;return;}
    r0=0;
}
static void join_job(int try_only){
    uint32_t object=r0,out=r1,error=check_object(object,THREAD_BYTES);
    if(error){r0=error;return;}
    ult_job *j=job_at(object);
    if(!j){r0=U_STATE;return;}
    if(out && !vita_mem_ptr(out,4)){r0=U_NULL;return;}
    if(j->joining || j->uid==vita_thread_self()){r0=U_BUSY;return;}
    if(try_only && !j->done){r0=U_AGAIN;return;}
    j->joining=1;int status=0;int rc=vita_thread_wait_end(j->uid,0,&status);
    if(rc){j->joining=0;r0=(uint32_t)rc;return;}
    if(out)vita_write32(out,j->result);
    vita_thread_delete(j->uid);ult_runtime *rt=runtime_at(j->runtime);if(rt && rt->active)rt->active--;
    vita_write16(object+160,3);j->object=0;j->joining=0;r0=0;
}
void vita_hle_sceUltUlthreadJoin(void){join_job(0);}
void vita_hle_sceUltUlthreadTryJoin(void){join_job(1);}
void vita_hle_sceUltUlthreadRuntimeDestroy(void){
    ult_runtime *rt=runtime_at(r0);
    if(!rt){r0=U_STATE;return;}
    if(rt->active){r0=U_BUSY;return;}
    vita_write32(rt->object,0);memset(rt,0,sizeof(*rt));r0=0;
}

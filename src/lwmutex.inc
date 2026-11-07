/* Lightweight-mutex bring-up overlay, independently implemented from the
 * VitaSDK ABI declarations. Included in the pinned MIT threadmgr source.
 * The guest work area is owner@0, count@8, attributes@12, uid@16.
 */
#define LWMUTEX_WORK_SIZE 32
#define LW_UNKNOWN 0x80028181u
#define LW_RECURSIVE 0x80028182u
#define LW_OVERFLOW 0x80028183u
#define LW_UNDERFLOW 0x80028184u
#define LW_NOT_OWNED 0x80028186u
#define LW_BAD_COUNT 0x8002000Fu
#define LW_MAX 512
typedef struct {uint32_t work, uid, attr, initial, waiters; char name[32];} port_lwmutex;
static port_lwmutex port_lw[LW_MAX];
static uint32_t port_lw_uid=0x41000000u;

static port_lwmutex *port_lw_find(uint32_t work) {
    if(!vita_mem_ptr(work,LWMUTEX_WORK_SIZE))return NULL;
    uint32_t uid=vita_read32(work+16);
    for(unsigned i=0;i<LW_MAX;i++)if(port_lw[i].work==work && port_lw[i].uid==uid)return &port_lw[i];
    return NULL;
}
void vita_hle_sceKernelCreateLwMutex(void) {
    uint32_t work=r0,attr=r2,count=r3;
    if(!vita_mem_ptr(work,LWMUTEX_WORK_SIZE)){r0=SCE_KERNEL_ERROR_INVALID_ARGUMENT;return;}
    if((int32_t)count<0 || (count>1 && !(attr&2))){r0=LW_BAD_COUNT;return;}
    if(port_lw_find(work)){r0=0x80028003u;return;}
    port_lwmutex *m=NULL;
    for(unsigned i=0;i<LW_MAX;i++)if(!port_lw[i].work){m=&port_lw[i];break;}
    if(!m){r0=SCE_KERNEL_ERROR_NO_MEMORY;return;}
    memset(m,0,sizeof(*m));m->work=work;m->uid=++port_lw_uid;m->attr=attr;m->initial=count;
    copy_name(m->name,r1);
    memset(vita_mem_ptr(work,LWMUTEX_WORK_SIZE),0,LWMUTEX_WORK_SIZE);
    vita_write32(work,count?vita_thread_self():0);
    vita_write32(work+8,count);vita_write32(work+12,attr);vita_write32(work+16,m->uid);
    r0=0;
}
static int lw_lock(uint32_t work,uint32_t count,uint32_t timeout,int try_only) {
    port_lwmutex *m=port_lw_find(work);
    if(!m)return (int)LW_UNKNOWN;
    if((int32_t)count<=0 || (count>1 && !(m->attr&2)))return (int)LW_BAD_COUNT;
    uint32_t me=vita_thread_self(),uid=m->uid;
    for(;;){
        m=port_lw_find(work);if(!m || m->uid!=uid)return (int)0x80028006u;
        uint32_t held=vita_read32(work+8),owner=vita_read32(work);
        if(!held || owner==me){
            if(held && !(m->attr&2))return (int)LW_RECURSIVE;
            if(held>0x7FFFFFFFu || count>0x7FFFFFFFu-held)return (int)LW_OVERFLOW;
            vita_write32(work,me);vita_write32(work+8,held+count);return 0;
        }
        if(try_only)return (int)SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN;
        m->waiters++;
        int rc=vita_sched_block(VW_MUTEX,work,timeout);
        m=port_lw_find(work);if(!m || m->uid!=uid)return (int)0x80028006u;
        if(m->waiters)m->waiters--;
        if(rc<0)return (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT;
    }
}
static int port_lw_unlock(uint32_t work,uint32_t count) {
    if(!port_lw_find(work))return (int)LW_UNKNOWN;
    if((int32_t)count<=0)return (int)LW_BAD_COUNT;
    if(vita_read32(work)!=vita_thread_self())return (int)LW_NOT_OWNED;
    uint32_t held=vita_read32(work+8);
    if(count>held)return (int)LW_UNDERFLOW;
    vita_write32(work+8,held-count);
    if(held==count){vita_write32(work,0);vita_sched_wake(VW_MUTEX,work,0);}
    return 0;
}
static void lw_unlock(uint32_t work,uint32_t count){(void)port_lw_unlock(work,count);}
void vita_hle_sceKernelLockLwMutex(void){r0=(uint32_t)lw_lock(r0,r1,timeout_us(r2),0);}
void vita_hle_sceKernelLockLwMutexCB(void){vita_hle_sceKernelLockLwMutex();}
void vita_hle_sceKernelTryLockLwMutex(void){r0=(uint32_t)lw_lock(r0,r1,0,1);}
void vita_hle_sceKernelUnlockLwMutex(void){r0=(uint32_t)port_lw_unlock(r0,r1);}
void vita_hle_sceKernelUnlockLwMutex2(void){vita_hle_sceKernelUnlockLwMutex();}
void vita_hle_sceKernelDeleteLwMutex(void){
    uint32_t work=r0;port_lwmutex *m=port_lw_find(work);
    if(!m){r0=LW_UNKNOWN;return;}
    m->work=0;vita_write32(work+16,0xFFFFFFFFu);vita_sched_wake(VW_MUTEX,work,1);r0=0;
}
void vita_hle_sceKernelGetLwMutexInfo(void){
    uint32_t work=r0,out=r1;port_lwmutex *m=port_lw_find(work);
    if(!m){r0=LW_UNKNOWN;return;}
    if(!vita_mem_ptr(out,4)){r0=SCE_KERNEL_ERROR_INVALID_ARGUMENT;return;}
    uint32_t size=vita_read32(out),n=size<64?size:64;
    if(size<4 || !vita_mem_ptr(out,n)){r0=SCE_KERNEL_ERROR_INVALID_ARGUMENT;return;}
    uint8_t info[64]={0};
    uint32_t words[16]={64,m->uid};
    memcpy(info,words,8);memcpy(info+8,m->name,32);
    words[0]=m->attr;words[1]=work;words[2]=m->initial;
    words[3]=vita_read32(work+8);words[4]=words[3]?vita_read32(work):0xFFFFFFFFu;words[5]=m->waiters;
    memcpy(info+40,words,24);memcpy(vita_mem_ptr(out,n),info,n);r0=0;
}

/* Local, permanently signed-out NP profile. No accounts, tickets or network
 * transport are fabricated. Manager ABI/error codes: Vita3K NP manager source;
 * lookup init arguments: measured PCSE00638 call site 813D0D1A.
 * Lookup's unusual/error cases still need firmware comparison. */
#include "vitarecomp/recomp_rt.h"
#include "vitarecomp/hle.h"
#include "vitarecomp/dispatch.h"
#include <string.h>
#define NP_ALREADY 0x80550001u
#define NP_UNINITIALIZED 0x80550002u
#define NP_ARGUMENT 0x80550003u
#define MANAGER_UNINITIALIZED 0x80550502u
#define MANAGER_ARGUMENT 0x80550503u
#define MANAGER_NO_ID 0x80550509u
static int initialized,lookup_initialized,basic_initialized,pending,checking;
static uint32_t service_callback,service_data;
static uint32_t message_callback,message_data;
static int valid(uint32_t p,uint32_t size){return p && vita_mem_avail(p)>=size;}
void vita_hle_sceNpInit(void) {
    if(initialized){r0=NP_ALREADY;return;}
    if(r1 || (r0 && !valid(r0,12))){r0=NP_ARGUMENT;return;}
    if(r0) {
        uint32_t id=vita_read32(r0);
        if(id && !valid(id,12)){r0=NP_ARGUMENT;return;}
    }
    initialized=1;lookup_initialized=0;pending=0;service_callback=service_data=0;
    vita_hle_trace("NP: local signed-out profile initialized; no PSN transport");r0=0;
}
void vita_hle_sceNpTerm(void) {
    initialized=lookup_initialized=basic_initialized=pending=0;
    service_callback=service_data=message_callback=message_data=0;r0=0;
}
void vita_hle_sceNpLookupInit(void) {
    if(!initialized){r0=NP_UNINITIALIZED;return;}
    if(lookup_initialized){r0=NP_ALREADY;return;}
    if(!r0 || r3){r0=NP_ARGUMENT;return;}
    lookup_initialized=1;r0=0;
}
void vita_hle_sceNpLookupTerm(void) {
    if(!lookup_initialized){r0=NP_UNINITIALIZED;return;}
    lookup_initialized=0;r0=0;
}
void vita_hle_sceNpGetServiceState(void) {
    if(!initialized){r0=MANAGER_UNINITIALIZED;return;}
    if(!valid(r0,4)){r0=MANAGER_ARGUMENT;return;}
    vita_write32(r0,1);r0=0; /* SCE_NP_SERVICE_STATE_SIGNED_OUT */
}
void vita_hle_sceNpManagerGetNpId(void) {
    if(!initialized){r0=MANAGER_UNINITIALIZED;return;}
    if(!valid(r0,36)){r0=MANAGER_ARGUMENT;return;}
    r0=MANAGER_NO_ID; /* Leave the destination unchanged on failure. */
}
void vita_hle_sceNpRegisterServiceStateCallback(void) {
    if(!initialized){r0=MANAGER_UNINITIALIZED;return;}
    if(!r0){r0=MANAGER_ARGUMENT;return;}
    service_callback=r0;service_data=r1;pending=1;r0=0;
}
void vita_hle_sceNpUnregisterServiceStateCallback(void) {
    if(!initialized){r0=MANAGER_UNINITIALIZED;return;}
    service_callback=service_data=0;pending=0;r0=0;
}
void vita_hle_sceNpCheckCallback(void) {
    if(!initialized){r0=MANAGER_UNINITIALIZED;return;}
    if(pending && service_callback && !checking) {
        uint32_t entry=service_callback,data=service_data;
        uint32_t regs[15]={r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,sp,lr};
        int flags[9]={flag_n,flag_z,flag_c,flag_v,flag_ge,fpscr_n,fpscr_z,fpscr_c,fpscr_v};
        vfp_reg saved[64];memcpy(saved,vfp_s,sizeof saved);
        pending=0;checking=1;r0=1;r1=0;r2=data;
        vita_dispatch(0,entry);
        checking=0;memcpy(vfp_s,saved,sizeof saved);
        r0=regs[0];r1=regs[1];r2=regs[2];r3=regs[3];r4=regs[4];r5=regs[5];r6=regs[6];r7=regs[7];
        r8=regs[8];r9=regs[9];r10=regs[10];r11=regs[11];r12=regs[12];sp=regs[13];lr=regs[14];
        flag_n=flags[0];flag_z=flags[1];flag_c=flags[2];flag_v=flags[3];flag_ge=flags[4];
        fpscr_n=flags[5];fpscr_z=flags[6];fpscr_c=flags[7];fpscr_v=flags[8];
    }
    r0=0;
}
/* Empty local social cache while signed out. No messages are delivered and
 * registration does not imply that remote sending/receiving is available. */
void vita_hle_sceNpBasicInit(void) {
    if(!initialized){r0=NP_UNINITIALIZED;return;}
    if(basic_initialized){r0=NP_ALREADY;return;}
    if(r0){r0=0x80551D02u;return;}
    basic_initialized=1;message_callback=message_data=0;r0=0;
}
void vita_hle_sceNpBasicTerm(void) {
    if(!basic_initialized){r0=NP_UNINITIALIZED;return;}
    basic_initialized=0;message_callback=message_data=0;r0=0;
}
void vita_hle_sceNpBasicRegisterInGameDataMessageHandler(void) {
    if(!basic_initialized){r0=NP_UNINITIALIZED;return;}
    if(!r0 || r2){r0=0x80551D02u;return;}
    message_callback=r0;message_data=r1;r0=0;
}
void vita_hle_sceNpBasicUnregisterInGameDataMessageHandler(void) {
    if(!basic_initialized){r0=NP_UNINITIALIZED;return;}
    message_callback=message_data=0;r0=0;
}
void vita_hle_sceNpBasicCheckCallback(void){r0=basic_initialized?0:NP_UNINITIALIZED;}
static void local_list_count(void) {
    if(!basic_initialized){r0=NP_UNINITIALIZED;return;}
    if(!valid(r0,4)){r0=0x80551D02u;return;}
    vita_write32(r0,0);r0=0;
}
void vita_hle_sceNpBasicGetFriendListEntryCount(void){local_list_count();}
void vita_hle_sceNpBasicGetBlockListEntryCount(void){local_list_count();}

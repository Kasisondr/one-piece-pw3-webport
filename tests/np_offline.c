#include "host_fixture.h"
#include "vitarecomp/dispatch.h"
void vita_hle_sceNpInit(void);void vita_hle_sceNpTerm(void);
void vita_hle_sceNpLookupInit(void);void vita_hle_sceNpLookupTerm(void);
void vita_hle_sceNpGetServiceState(void);void vita_hle_sceNpManagerGetNpId(void);
void vita_hle_sceNpRegisterServiceStateCallback(void);void vita_hle_sceNpUnregisterServiceStateCallback(void);
void vita_hle_sceNpCheckCallback(void);
void vita_hle_sceNpBasicInit(void);void vita_hle_sceNpBasicTerm(void);
void vita_hle_sceNpBasicRegisterInGameDataMessageHandler(void);void vita_hle_sceNpBasicCheckCallback(void);
void vita_hle_sceNpBasicGetFriendListEntryCount(void);void vita_hle_sceNpBasicGetBlockListEntryCount(void);
static int calls;
static void callback(void) {
    assert(r0==1 && !r1 && r2==0x12345678);calls++;
    vita_hle_sceNpCheckCallback();assert(!r0); /* Recursive poll must not redispatch. */
    r4=0;sp=4;lr=0;flag_c=0;vfp_setu(31,0);
}
int main(void) {
    assert(!vita_mem_init(0x81000000,0x10000));
    r0=0x81000100;vita_hle_sceNpGetServiceState();assert(r0==0x80550502);
    r0=1;r1=r2=r3=0;vita_hle_sceNpLookupInit();assert(r0==0x80550002);
    r0=0xFFFFFFFF;r1=0;vita_hle_sceNpInit();assert(r0==0x80550003);
    r0=r1=0;vita_hle_sceNpInit();assert(!r0);
    r0=r1=0;vita_hle_sceNpInit();assert(r0==0x80550001);
    r0=0x81000100;vita_hle_sceNpGetServiceState();assert(!r0 && vita_read32(0x81000100)==1);
    r0=0;vita_hle_sceNpGetServiceState();assert(r0==0x80550503);
    vita_write32(0x81000200,0xDEADBEEF);r0=0x81000200;vita_hle_sceNpManagerGetNpId();assert(r0==0x80550509 && vita_read32(0x81000200)==0xDEADBEEF);
    r0=1;r1=0x10000100;r2=r3=0;vita_hle_sceNpLookupInit();assert(!r0);
    r0=1;r3=0;vita_hle_sceNpLookupInit();assert(r0==0x80550001);
    r0=vita_dispatch_host(callback);r1=0x12345678;vita_hle_sceNpRegisterServiceStateCallback();assert(!r0);
    r4=0x87654321;sp=0x8100F000;lr=0xABCD;flag_c=1;vfp_setu(31,0xF00DF00D);
    vita_hle_sceNpCheckCallback();assert(!r0 && calls==1);
    assert(r4==0x87654321 && sp==0x8100F000 && lr==0xABCD && flag_c==1 && vfp_getu(31)==0xF00DF00D);
    vita_hle_sceNpCheckCallback();assert(!r0 && calls==1);
    r0=vita_dispatch_host(callback);r1=0x12345678;vita_hle_sceNpRegisterServiceStateCallback();
    vita_hle_sceNpUnregisterServiceStateCallback();vita_hle_sceNpCheckCallback();assert(calls==1);
    r0=0;vita_hle_sceNpBasicInit();assert(!r0);
    r0=vita_dispatch_host(callback);r1=123;r2=0;vita_hle_sceNpBasicRegisterInGameDataMessageHandler();assert(!r0);
    vita_hle_sceNpBasicCheckCallback();assert(!r0 && calls==1);
    r0=0x81000300;vita_hle_sceNpBasicGetFriendListEntryCount();assert(!r0 && !vita_read32(0x81000300));
    r0=0x81000300;vita_hle_sceNpBasicGetBlockListEntryCount();assert(!r0 && !vita_read32(0x81000300));
    vita_hle_sceNpBasicTerm();assert(!r0);
    vita_hle_sceNpLookupTerm();assert(!r0);vita_hle_sceNpLookupTerm();assert(r0==0x80550002);
    vita_hle_sceNpTerm();vita_hle_sceNpCheckCallback();assert(r0==0x80550502);
    assert(!vita_mem_bad_access);puts("NP: signed-out state, no fabricated ID, lifecycle and preserved callback context passed");return 0;
}

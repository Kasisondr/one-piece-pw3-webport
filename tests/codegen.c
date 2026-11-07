#include "host_fixture.h"
#include "ldrd_fixture.inc"
#include "game_codegen_fixture.inc"
int main(void){
    assert(!vita_mem_init(0x81000000u,0x10000));
    r0=0x81000100u;vita_write32(r0+0x38,0);vita_write32(r0+0x3C,0xDEADBEEFu);
    regression_ldrd();assert(r0==0 && r1==0xDEADBEEFu && vita_mem_bad_access==0);
    r7=0x81000200u;r6=0x10;vita_write32(r7+r6,0x1234);vita_write32(r7+r6+4,0xABCD);
    regression_reverse();assert(r7==0x1234 && r8==0xABCD && vita_mem_bad_access==0);
#if PORT_HAS_ERROR_TABLE
    const int errors[30]={-33,-26,-28,-28,-28,-28,-28,-28,-38,-28,-28,-29,-33,-1,-28,-8,-34,-6,-28,-26,-27,-6,-28,-5,-28,-28,-28,-35,-28,-23};
    for(unsigned i=0;i<30;i++) {
        r0=0x80010001u+i;r2=0x12345678;vita_func_81307514();
        assert(r0==(uint32_t)errors[i] && r2==0x12345678);
    }
    r0=0;vita_func_81307514();assert(r0==0);
    r0=0x80010000;vita_func_81307514();assert(r0==(uint32_t)-28);
    r0=0x8001001F;vita_func_81307514();assert(r0==(uint32_t)-28);
    assert(!vita_mem_bad_access);
    puts("Retail TBH: all 30 firmware error cases and boundary guards passed");
#endif
    puts("LDRD: both values loaded from original base before destination writes");return 0;
}

/* Additional firmware queries, using the pinned toolkit's GXP layout.
 * No renderer or game shader is replaced by this query.
 */
#include "vitarecomp/recomp_rt.h"
void vita_hle_sceGxmProgramParameterGetIndex(void){
    uint32_t program=r0,param=r1;r0=0;
    if(!vita_mem_ptr(program,0x98))return;
    uint64_t table=(uint64_t)program+0x28u+vita_read32(program+0x28);
    uint32_t count=vita_read32(program+0x24);
    if(table>0xFFFFFFFFu || param<table || (param-table)%16u)return;
    uint64_t index=(param-table)/16u;
    if(index<count && vita_mem_ptr(param,16))r0=(uint32_t)index;
}
void vita_hle_sceGxmProgramParameterIsRegFormat(void){
    uint32_t program=r0,param=r1;r0=0;
    if(!vita_mem_ptr(program,0x98) || !vita_mem_ptr(param,16))return;
    if(vita_read32(program+0x14)&1u)return; /* fragment programs */
    if((vita_read8(param+4)&15u)!=0)return; /* vertex attributes only */
    uint32_t reg=vita_read32(param+12);
    if(reg>=64)return;
    uint64_t table=(uint64_t)program+0x2Cu+vita_read32(program+0x2C);
    if(table>0xFFFFFFFFu || !vita_mem_ptr((uint32_t)table,28))return;
    uint32_t word=vita_read32((uint32_t)table+8+(reg>=32?4:0));
    r0=(word>>(reg&31))&1u;
}

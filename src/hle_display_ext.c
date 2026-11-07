#include "vitarecomp/recomp_rt.h"
/* VitaSDK: int sceDisplayGetRefreshRate(float *fps). */
void vita_hle_sceDisplayGetRefreshRate(void){
    if(!vita_mem_ptr(r0,4)){r0=0x80290001u;return;}
    vita_write32(r0,0x42700000u); /* IEEE754 60.0f */
    r0=0;
}

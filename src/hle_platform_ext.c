#include "vitarecomp/recomp_rt.h"
#include "ult_runtime.h"
/* Present the handheld Vita device profile, whose panel is 960x544. */
void vita_hle_sceKernelIsPSVitaTV(void){r0=0;}
void vita_hle_sceKernelGetThreadExitStatus(void){r0=(uint32_t)port_thread_exit_status(r0,r1);}
/* Same three-argument copy ABI as sceClibMemcpy, including returning dst.
 * Firmware adds overlap diagnostics; retain the runtime's bounded,
 * overlap-tolerant copy here. Zero length touches no
 * address, including null. This is not the four-argument memcpy_s API. */
void vita_hle_sceClibMemcpy(void);
void vita_hle_sceClibMemcpy_safe(void){if(r2)vita_hle_sceClibMemcpy();}

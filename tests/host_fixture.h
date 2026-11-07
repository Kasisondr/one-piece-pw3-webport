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

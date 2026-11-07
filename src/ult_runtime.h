#ifndef OPW3_ULT_RUNTIME_H
#define OPW3_ULT_RUNTIME_H
#include <stdint.h>
/* Internal scheduler extension: attach an existing guest stack before start. */
int port_thread_stack(uint32_t uid, uint32_t base, uint32_t size);
int port_thread_exit_status(uint32_t uid, uint32_t out);
int port_thread_start_deferred(uint32_t uid);
#endif

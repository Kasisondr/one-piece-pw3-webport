/* Browser host for statically recompiled Vita code; no ARM interpreter.
 * Guest code runs on the Emscripten pthread worker. Browser operations are
 * synchronously proxied to the UI thread, which owns the canvas and input.
 */
#include "../external/vitarecomp/host/window.h"
#include <emscripten.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <threads.h>
static double next_frame;
int vita_window_open(const char *title, int width, int height) {
    (void)title; (void)width; (void)height;
    next_frame = emscripten_get_now();
    return 0;
}
void vita_window_fullscreen(void) {} /* Requires a browser user gesture; UI owns this. */
void *vita_window_native(void) { return 0; }
void vita_window_drawn(int width, int height) { (void)width; (void)height; }
void vita_window_title(const char *text) { (void)text; }
void vita_window_present(const uint8_t *pixels, int width, int height, int pitch) {
    MAIN_THREAD_EM_ASM({
        const present = globalThis.vitaBrowserHost?.present;
        if (present) present(HEAPU8, $0, $1, $2, $3);
    }, pixels, width, height, pitch);
}
/* One snapshot for each HLE request; the UI always returns neutral input
 * on focus loss. No JS callback is invoked from a guest pthread directly. */
static void snapshot(int values[8]) {
    MAIN_THREAD_EM_ASM({
        const s = globalThis.vitaBrowserHost?.input?.() ?? new Array(0,128,128,128,128,0,0,0);
        for (let i=0;i<8;i++) HEAP32[($0 >> 2)+i] = s[i];
    }, values);
}
uint32_t vita_window_buttons(void) { int s[8]; snapshot(s); return (uint32_t)s[0]; }
void vita_window_sticks(uint8_t xy[4]) {
    int s[8]; snapshot(s); for (int i=0;i<4;i++) xy[i]=(uint8_t)s[i+1];
}
int vita_window_touch(int *x, int *y) {
    int s[8]; snapshot(s); *x=s[6]; *y=s[7]; return s[5];
}
int vita_window_closed(void) { return 0; }
uint32_t vita_window_pace_left(void) {
    double us=(next_frame-emscripten_get_now())*1000.0;
    return us>0 ? (uint32_t)us : 0;
}
void vita_window_pace(void) {
    next_frame += 1000.0/60.0;
    double now=emscripten_get_now(), ms=next_frame-now;
    if(ms>0) {
        struct timespec t={0,(long)(ms*1000000.0)};
        thrd_sleep(&t,0);
    } else if(ms < -33.3) next_frame=now;
}

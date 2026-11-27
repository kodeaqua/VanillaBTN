/* The system: the host-debugger poll and the display on and off. */
#ifdef __psyz
#include <stdlib.h>
#endif
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include "types.h"
#include "cpu_asm.h"
#include "system.h"

/* Fault paths stop at the PS1 debugger or terminate the native process. */
s32 host_poll(void) {
#ifdef __psyz
    abort();
#else
    pollhost();
    return 1;
#endif
}

/* Switch the video output on and off. */
void display_on(void) {
    SetDispMask(1);
}

void display_off(void) {
    SetDispMask(0);
}

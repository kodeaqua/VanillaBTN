#include "system.h"
#include <libgpu.h>
#include <stdlib.h>
#ifndef NDEBUG
#include <stdarg.h>
#include <stdio.h>

void FntPrint(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}
#endif

s32 host_poll(void) {
    /* Original fatal loops wait indefinitely for a PS1 debugger. Native
     * unsupported/invalid paths must stop instead of hanging the host. */
    abort();
}

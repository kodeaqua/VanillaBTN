#include <libgpu.h>
#ifndef NDEBUG
#include "host_debug.h"
#include <stdarg.h>
#include <stdio.h>

static int debug_print(const char *format, va_list args) {
    char message[512];
    int written = vsnprintf(message, sizeof(message), format, args);
    if (written >= 0) HOST_DEBUG_WARN("GAME", "%s", message);
    return written;
}

void FntPrint(const char *format, ...) {
    if (!format) return;
    va_list args;
    va_start(args, format);
    debug_print(format, args);
    va_end(args);
}

void FntFlush(int id) {
    (void)id;
    fflush(stderr);
}

void FntLoad(int tx, int ty) {
    (void)tx;
    (void)ty;
}

int FntOpen(int x, int y, int w, int h, int isbg, int n) {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)isbg;
    (void)n;
    return 0;
}

void SetDumpFnt(int id) {
    (void)id;
}

int KanjiFntOpen(int x, int y, int w, int h, int dx, int dy, int cx, int cy, int isbg, int n) {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)dx;
    (void)dy;
    (void)cx;
    (void)cy;
    (void)isbg;
    (void)n;
    return 0;
}

void KanjiFntClose(void) {
}

int KanjiFntPrint(const char *format, ...) {
    if (!format) return 0;
    int written;
    va_list args;
    va_start(args, format);
    written = debug_print(format, args);
    va_end(args);
    return written;
}

void KanjiFntFlush(int id) {
    (void)id;
    fflush(stderr);
}
#endif

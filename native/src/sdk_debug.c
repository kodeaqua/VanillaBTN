#include <libgpu.h>
#ifndef NDEBUG
#include <stdarg.h>
#include <stdio.h>

void FntPrint(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
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
    int written;
    va_list args;
    va_start(args, format);
    written = vfprintf(stderr, format, args);
    va_end(args);
    return written;
}

void KanjiFntFlush(int id) {
    (void)id;
    fflush(stderr);
}
#endif

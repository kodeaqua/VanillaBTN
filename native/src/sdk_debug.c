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
#endif

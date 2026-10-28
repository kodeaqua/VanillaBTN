#include <libgpu.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

s32 FntPrint(const char *format, ...) {
    if (!format) {
        return -1;
    }
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        return -1;
    }
    char stack_buf[2048];
    char *buf = stack_buf;
    if ((size_t)needed >= sizeof(stack_buf)) {
        buf = (char *)malloc((size_t)needed + 1);
        if (!buf) {
            va_end(args);
            return -1;
        }
    }
    vsnprintf(buf, (size_t)needed + 1, format, args);
    va_end(args);
    fputs(buf, stderr);
    fflush(stderr);
    if (buf != stack_buf) {
        free(buf);
    }
    return (s32)needed;
}

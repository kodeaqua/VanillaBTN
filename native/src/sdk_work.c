#include "sdk_work.h"
#include "main.h"
#include <libapi.h>
#include <stdint.h>
#include <stdlib.h>

static unsigned char *base;
static size_t capacity, used;

bool sdk_work_bind(void *memory, size_t size) {
    if ((memory == NULL) != (size == 0) || ((uintptr_t)memory & 3) || (size & 3)) return false;
    base = memory;
    capacity = size;
    used = 0;
    return true;
}

void *gs_work_alloc(s32 size) {
    /* The original API cannot report allocation failure. Stop at the platform
     * boundary instead of returning an invalid pointer or corrupting packets. */
    if (!base || size < 0 || (size & 3) || (size_t)size > capacity - used) abort();
    void *packet = base + used;
    used += (size_t)size;
    return packet;
}

void gs_work_release(void *end) {
    uintptr_t address = (uintptr_t)end, start = (uintptr_t)base;
    if (!base || address < start || address - start > capacity || (address & 3)) abort();
    used = address - start;
}

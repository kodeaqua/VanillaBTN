#include <stdlib.h>
#include "sdk_memory.h"
#include <string.h>

_Alignas(16) unsigned char sdk_main_ram[SDK_RAM_SIZE];

void *sdk_memory_resolve(uint32_t address, size_t size) {
    uint32_t segment = address & 0xE0000000u;
    if (segment != 0 && segment != 0x80000000u && segment != 0xA0000000u) return NULL;
    uint32_t offset = address & 0x1FFFFFFFu;
    if (offset >= SDK_RAM_SIZE || size > SDK_RAM_SIZE - offset) return NULL;
    return sdk_main_ram + offset;
}

void sdk_memory_clear(void) { memset(sdk_main_ram, 0, sizeof(sdk_main_ram)); }

/* Native storage has the original retail machine's fixed 2 MB capacity. */
void SetMem(long megabytes) {
    if (megabytes != 2) abort();
}

void *sdk_memory_field(void *base, size_t native_offset, uint32_t ps1_offset,
                       size_t size) {
    uintptr_t address = (uintptr_t)base;
    if (address <= UINT32_MAX) {
        if (ps1_offset > UINT32_MAX - address) return NULL;
        return sdk_memory_resolve((uint32_t)address + ps1_offset, size);
    }
    if (native_offset > UINTPTR_MAX - address ||
        size > UINTPTR_MAX - (address + native_offset)) return NULL;
    return (unsigned char *)base + native_offset;
}

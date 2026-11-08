#include "sdk_memory.h"
#include "overlay_regions.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "Check failed at line %d: %s\n", __LINE__, #condition); \
    exit(1); \
} } while (0)

int main(void) {
    sdk_memory_clear();
    CHECK(sdk_memory_resolve(0x80000000, 4) == sdk_main_ram);
    CHECK(sdk_memory_resolve(0xA0000010, 8) == sdk_main_ram + 16);
    CHECK(sdk_memory_resolve(0x1FFFFF, 1) == sdk_main_ram + SDK_RAM_SIZE - 1);
    CHECK(sdk_memory_resolve(0x1FFFFF, 2) == NULL);
    CHECK(sdk_memory_resolve(0x1F800000, 4) == NULL);
    CHECK(sdk_memory_resolve(0xC0000000, 4) == NULL);
    CHECK(sdk_memory_resolve(0x80200000, 0) == NULL);
    CHECK(sdk_memory_resolve(0x80000000, SIZE_MAX) == NULL);
    CHECK(overlay_region_table[OVERLAY_LOW] == sdk_memory_resolve(0x800B7E20, 4));
    CHECK(overlay_region_table[OVERLAY_HIGH] == sdk_memory_resolve(0x8012DDE8, 4));
    CHECK(overlay_region_table[OVERLAY_HIGH] - overlay_region_table[OVERLAY_LOW] == 0x75FC8);
    *(unsigned char *)sdk_memory_resolve(0x80000010, 1) = 0xAB;
    CHECK(*(unsigned char *)sdk_memory_resolve(0xA0000010, 1) == 0xAB);
    sdk_memory_clear();
    CHECK(sdk_main_ram[16] == 0);
    return 0;
}

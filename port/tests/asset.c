#include "compat/asset.h"
#include <stdio.h>

int main(void) {
    const u8 negative[5] = {0xCC, 0xFE, 0xFF, 0xFF, 0xFF};
    const u8 positive[4] = {0x78, 0x56, 0x34, 0x12};
    if (hm_asset_s32le(negative + 1) != -2 || hm_asset_s32le(positive) != 0x12345678) {
        fprintf(stderr, "Asset word endian/sign conversion failed\n");
        return 1;
    }
    return 0;
}

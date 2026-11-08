#include "sdk_gpu.h"
#include "texture_pack.h"
#include <assert.h>
#include <string.h>
#include <stddef.h>

_Static_assert(offsetof(ImageBank, images) == 4, "Bank image offset must match disc data");
_Static_assert(offsetof(BankImage, data) == 12, "Pixel offset must match disc data");

static Ps1Video video;

static void test_transfers(void) {
    RECT rect = {1021, 510, 3, 2};
    unsigned char bytes[16] = {0xAA, 0x1F, 0, 0xE0, 3, 0, 0x7C, 1, 0x80, 2, 0, 3, 0, 0xBB};
    unsigned char copy[16] = {0};
    sdk_gpu_bind(NULL);
    assert(LoadImage(&rect, (u_long *)(bytes + 1)) == -1);
    sdk_gpu_bind(&video);
    assert(LoadImage(&rect, (u_long *)(bytes + 1)) == 0);
    assert(video.vram[510][1021] == 0x001F);
    assert(video.vram[511][1021] == 0x8001);
    copy[0] = 0xAA; copy[13] = 0xBB;
    assert(StoreImage(&rect, (u_long *)(copy + 1)) == 0);
    assert(memcmp(bytes, copy, 14) == 0);
    rect.w = 4;
    assert(LoadImage(&rect, (u_long *)bytes) == -1);
    assert(StoreImage(&rect, (u_long *)copy) == -1);
    assert(DrawSync(0) == 0 && DrawSync(1) == 0);
}

static void test_original_loader(void) {
    /* One original ImageBank, one 2x1 raw block, two BGR555 pixels.
     * Call the actual game translation unit, not the native decoder. */
    unsigned char bank[24] = {0x0D,0,1,0, 4,0,0,4, 10,0,2,0, 2,0,1,0,
                              0x1F,0,0xE0,3};
    texture_pack_load(bank);
    assert(video.vram[2][10] == 0x001F && video.vram[2][11] == 0x03E0);
    texture_pack_load_at(bank, 3, 4, 20, 30);
    assert(video.vram[6][13] == 0x001F);
    bank[7] = 6; /* Raw block placed at alternate coordinates. */
    texture_pack_load_at(bank, 3, 4, 20, 30);
    assert(video.vram[32][30] == 0x001F);
    bank[4] = 5; bank[7] = 0;
    bank[16] = 252; /* RLE literal run of four bytes. */
    bank[17] = 0; bank[18] = 0x7C; bank[19] = 0x1F; bank[20] = 0x80;
    texture_pack_load(bank);
    assert(video.vram[2][10] == 0x7C00 && video.vram[2][11] == 0x801F);
    unsigned char expanded[4];
    assert(image_bank_upload(bank, expanded, 0, 40, 50) == 4);
    assert(video.vram[50][40] == 0x7C00 && video.vram[50][41] == 0x801F);
}

int main(void) {
    test_transfers();
    test_original_loader();
    sdk_gpu_bind(NULL);
    return 0;
}

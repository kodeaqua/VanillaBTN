#include "texture_pack.h"
#include "psx/vram.h"
#include <stdio.h>
#include <string.h>

#define CHECK(test)                                                                                \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "Failed at %d: %s\n", __LINE__, #test);                                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    /* Synthetic bank data exercises the original game decoder, not a rewritten
     * port decoder. All bytes are fixtures, unrelated to disc assets. */
    u32 storage[8] = {0};
    ImageBank *bank = (ImageBank *)storage;
    BankImage *image = bank->images;
    bank->type = 0xD;
    bank->count = 1;
    image->x = 5;
    image->y = 6;
    image->w = image->h = 2;
    image->sizeFlags = 0x04000008;
    const u8 raw[] = {1, 2, 3, 4, 5, 6, 7, 8};
    memcpy(image->data, raw, sizeof(raw));
    hm_vram_reset();
    texture_pack_load((u8 *)bank);
    CHECK(hm_vram_pixel(5, 6) == 0x0201 && hm_vram_pixel(6, 7) == 0x0807);
    texture_pack_load_at(bank, 10, 20, 0, 0);
    CHECK(hm_vram_pixel(15, 26) == 0x0201 && hm_vram_pixel(16, 27) == 0x0807);
    image->sizeFlags = 0x06000008; /* Alternative destination for flag bits 0..1 == 2. */
    texture_pack_load_at(bank, 10, 20, 30, 40);
    CHECK(hm_vram_pixel(35, 46) == 0x0201 && hm_vram_pixel(36, 47) == 0x0807);

    image->sizeFlags = 7;
    const u8 rle[] = {0xFC, 1, 2, 3, 4, 4, 0x7F};
    memcpy(image->data, rle, sizeof(rle));
    texture_pack_load((u8 *)bank);
    CHECK(hm_vram_pixel(5, 6) == 0x0201 && hm_vram_pixel(6, 6) == 0x0403);
    CHECK(hm_vram_pixel(5, 7) == 0x7F7F && hm_vram_pixel(6, 7) == 0x7F7F);
    u8 expanded[8] = {0};
    CHECK(image_bank_upload(bank, expanded, 0, 50, 60) == 8);
    CHECK(hm_vram_pixel(50, 60) == 0x0201 && hm_vram_pixel(51, 61) == 0x7F7F);
    CHECK(expanded[0] == 1 && expanded[7] == 0x7F);

    image->sizeFlags = 0x08000009;
    const u8 columns[] = {0xF8, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    memcpy(image->data, columns, sizeof(columns));
    texture_pack_load((u8 *)bank);
    CHECK(hm_vram_pixel(5, 6) == 0x3311 && hm_vram_pixel(5, 7) == 0x4422);
    CHECK(hm_vram_pixel(6, 6) == 0x7755 && hm_vram_pixel(6, 7) == 0x8866);
    return 0;
}

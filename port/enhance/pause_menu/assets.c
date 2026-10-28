#include "enhance/pause_menu/assets.h"
#include "psx/vram.h"
#include "texture_pack.h"
#include <stdlib.h>

static u32 hm_le32(const u8 *p) {
    return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
}
int hm_pause_assets_load(hm_pause_assets *assets, hm_disc *disc) {
    hm_disc_entry hdt, archive;
    u8 offsets[8];
    if (hm_disc_find(disc, "A_FILE.HDT;1", &hdt) != HM_DISC_OK ||
        hm_disc_find(disc, "A_FILE.BIN;1", &archive) != HM_DISC_OK ||
        hm_disc_read(disc, &hdt, 0, offsets, sizeof(offsets)) != HM_DISC_OK)
        return 0;
    u32 start = hm_le32(offsets), end = hm_le32(offsets + 4);
    if (end <= start || end > archive.size)
        return 0;
    size_t bytes = end - start;
    if (bytes > 65536)
        bytes = 65536; /* Only the initial resident UI texture bank. */
    u8 *data = malloc(bytes);
    if (!data)
        return 0;
    int ok = 0;
    if (hm_disc_read(disc, &archive, start, data, bytes) != HM_DISC_OK || bytes < 4 ||
        data[0] != 13)
        goto done;
    unsigned count = (unsigned)data[2] | (unsigned)data[3] << 8;
    size_t offset = 4;
    if (!count || count > 256)
        goto done;
    for (unsigned i = 0; i < count; ++i) {
        if (offset > bytes || bytes - offset < 12)
            goto done;
        size_t payload = hm_le32(data + offset) & 0xFFFFFFu;
        unsigned width = data[offset + 8] | (unsigned)data[offset + 9] << 8;
        unsigned height = data[offset + 10] | (unsigned)data[offset + 11] << 8;
        if (!width || width > 256 || !height || height > 512 || payload > bytes - offset - 12)
            goto done;
        offset += 12 + ((payload + 3) & ~(size_t)3);
        if (offset > bytes)
            goto done;
    }
    /* Startup only, before the game's own graph initialization. Decode through
     * the game's original texture-bank implementation, then retain its exact
     * English glyphs and palette in a private cache, independent of overlays. */
    texture_pack_load(data);
    for (unsigned y = 0; y < HM_PAUSE_FONT_H; ++y)
        for (unsigned x = 0; x < HM_PAUSE_FONT_W; ++x) {
            u16 packed = hm_vram_pixel(896 + x / 4, y);
            unsigned color = (packed >> ((x % 4) * 4)) & 15;
            assets->font[y * HM_PAUSE_FONT_W + x] = hm_vram_pixel(896 + color, 252);
            assets->font_light[y * HM_PAUSE_FONT_W + x] = hm_vram_pixel(896 + color, 250);
        }
    ok = 1;
done:
    hm_vram_reset();
    free(data);
    return ok;
}

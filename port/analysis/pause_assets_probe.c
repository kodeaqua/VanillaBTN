/* Local-only asset decoding check. Never starts the game or opens a window. */
#include "enhance/pause_menu/assets.h"
#include "psx/vram.h"
#include <limits.h>
#include <stdio.h>
static u8 hm_preview[320 * 240 * 4];
static void hm_write32(u8 *out, u32 n) {
    for (unsigned i = 0; i < 4; ++i)
        out[i] = (u8)(n >> (i * 8));
}
static int hm_preview_write(const char *path) {
    u8 header[54] = {'B', 'M'}, row[960];
    hm_write32(header + 2, 54 + 320 * 240 * 3);
    hm_write32(header + 10, 54);
    hm_write32(header + 14, 40);
    hm_write32(header + 18, 320);
    hm_write32(header + 22, 240);
    header[26] = 1;
    header[28] = 24;
    FILE *file = fopen(path, "wb");
    if (!file)
        return 0;
    int ok = fwrite(header, 1, sizeof(header), file) == sizeof(header);
    for (int y = 239; y >= 0 && ok; --y) {
        for (unsigned x = 0; x < 320; ++x) {
            const u8 *p = hm_preview + ((size_t)y * 320 + x) * 4;
            row[x * 3] = p[2];
            row[x * 3 + 1] = p[1];
            row[x * 3 + 2] = p[0];
        }
        ok = fwrite(row, 1, sizeof(row), file) == sizeof(row);
    }
    if (fclose(file))
        ok = 0;
    return ok;
}
static int hm_read(void *context, u64 offset, void *destination, size_t bytes) {
    return offset <= LONG_MAX && !fseek(context, (long)offset, SEEK_SET) &&
           fread(destination, 1, bytes, context) == bytes;
}
int main(int argc, char **argv) {
    if (argc != 2 && argc != 4)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file || fseek(file, 0, SEEK_END))
        return 1;
    long bytes = ftell(file);
    hm_disc disc;
    hm_pause_assets assets;
    hm_disc_result status =
        bytes < 0 ? HM_DISC_IO : hm_disc_mount(&disc, (hm_disc_stream){file, (u64)bytes, hm_read});
    if (status != HM_DISC_OK || !hm_pause_assets_load(&assets, &disc)) {
        fprintf(stderr, "Pause asset decode failed (disc status %d)\n", status);
        fclose(file);
        return 1;
    }
    unsigned opaque = 0;
    for (size_t i = 0; i < sizeof(assets.font) / sizeof(assets.font[0]); ++i)
        if (assets.font[i])
            ++opaque;
    if (!opaque || opaque == HM_PAUSE_FONT_W * HM_PAUSE_FONT_H) {
        fprintf(stderr, "Invalid pause font coverage: %u\n", opaque);
        fclose(file);
        return 1;
    }
    for (unsigned y = 0; y < HM_VRAM_HEIGHT; ++y)
        for (unsigned x = 0; x < HM_VRAM_WIDTH; ++x)
            if (hm_vram_pixel(x, y)) {
                fclose(file);
                return 1;
            }
    fclose(file);
    if (argc == 4) {
        hm_pause_menu menu;
        hm_pause_open(&menu);
        hm_pause_draw(&assets, hm_preview, &menu, NULL);
        if (!hm_preview_write(argv[2]))
            return 1;
        for (size_t i = 0; i < sizeof(hm_preview); ++i)
            hm_preview[i] = 0;
        menu.page = HM_PAUSE_GRAPHICS;
        menu.selected = 1;
        /* Visual layout fixture values, not a live monitor or game session. */
        const hm_pause_view view = {
            .values = {"1", "Windowed", "1920 x 1080", "Off", "Off", "Off", "Nearest"},
            .detail = "Windowed"};
        hm_pause_draw(&assets, hm_preview, &menu, &view);
        if (!hm_preview_write(argv[3]))
            return 1;
    }
    printf("Original pause font cache decoded: %u opaque pixels; game VRAM restored to zero\n",
           opaque);
    return 0;
}

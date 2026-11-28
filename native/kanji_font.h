#ifndef VANILLABTN_KANJI_FONT_H
#define VANILLABTN_KANJI_FONT_H

enum {
    KANJI_MAX_WIDTH = 320,
    KANJI_MAX_HEIGHT = 240,
    KANJI_VRAM_HALFWORDS = (KANJI_MAX_WIDTH / 4) * KANJI_MAX_HEIGHT,
};

/* Rasterize Shift-JIS text into packed 4-bit PS1 VRAM pixels (4 pixels per u16). */
int vanillabtn_kanji_rasterize(const char *sjis, int width, int height, unsigned short *out_4bit);

#endif

#include <stdio.h>
#include "kanji_font.h"

static unsigned short vram[KANJI_VRAM_HALFWORDS];

static int count_nonzero(int width, int height) {
    int total = (width / 4) * height;
    int count = 0;
    for (int i = 0; i < total; i++) {
        if (vram[i] != 0) {
            count++;
        }
    }
    return count;
}

int main(void) {
    if (!vanillabtn_kanji_rasterize("", 320, 240, vram) || count_nonzero(320, 240) != 0) {
        fputs("Empty string must produce a blank bitmap\n", stderr);
        return 1;
    }

    /* Shift-JIS "DEBUG" + Japanese menu title from 08_DEBUG. */
    const char *sample = "\202c\202d\202a\202t\202f\n\201u\203v\203\215\203O\203\211\203\200\201v";
    if (!vanillabtn_kanji_rasterize(sample, 320, 240, vram) || count_nonzero(320, 240) == 0) {
        fputs("Shift-JIS text must produce visible pixels\n", stderr);
        return 1;
    }

    if (vanillabtn_kanji_rasterize("bad", 321, 240, vram) != 0) {
        fputs("Unaligned width must be rejected\n", stderr);
        return 1;
    }
    return 0;
}

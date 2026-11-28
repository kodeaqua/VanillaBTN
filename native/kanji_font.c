#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include "kanji_font.h"

enum {
    CP_SHIFT_JIS = 932,
    KANJI_FONT_HEIGHT = 12,
    KANJI_MAX_WIDE = 2048,
};

int vanillabtn_kanji_rasterize(const char *sjis, int width, int height, unsigned short *out_4bit) {
    if (sjis == NULL || out_4bit == NULL || width <= 0 || height <= 0 ||
        width > KANJI_MAX_WIDTH || height > KANJI_MAX_HEIGHT || (width & 3) != 0) {
        return 0;
    }

    size_t halfwords = (size_t)(width / 4) * (size_t)height;
    memset(out_4bit, 0, halfwords * sizeof(*out_4bit));
    if (sjis[0] == '\0') {
        return 1;
    }

    wchar_t wide[KANJI_MAX_WIDE];
    int wide_len = MultiByteToWideChar(CP_SHIFT_JIS, 0, sjis, -1, wide, KANJI_MAX_WIDE);
    if (wide_len <= 1) {
        return 1;
    }

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; /* top-down DIB */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    HDC dc = CreateCompatibleDC(NULL);
    if (dc == NULL) {
        return 0;
    }
    HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (bmp == NULL || bits == NULL) {
        if (bmp != NULL) {
            DeleteObject(bmp);
        }
        DeleteDC(dc);
        return 0;
    }

    HGDIOBJ old_bmp = SelectObject(dc, bmp);
    HFONT font = CreateFontW(-KANJI_FONT_HEIGHT, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, L"MS Gothic");
    HGDIOBJ old_font = font != NULL ? SelectObject(dc, font) : NULL;

    memset(bits, 0, (size_t)width * (size_t)height * 4);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));

    RECT bounds = {0, 0, width, height};
    DrawTextW(dc, wide, wide_len - 1, &bounds, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_EXPANDTABS);
    GdiFlush();

    const unsigned int *pixels = (const unsigned int *)bits;
    int row_words = width / 4;
    for (int y = 0; y < height; y++) {
        const unsigned int *src = pixels + (size_t)y * (size_t)width;
        unsigned short *dst = out_4bit + (size_t)y * (size_t)row_words;
        for (int x = 0; x < width; x += 4) {
            unsigned int p0 = (src[x] & 0xFFU) >> 4;
            unsigned int p1 = (src[x + 1] & 0xFFU) >> 4;
            unsigned int p2 = (src[x + 2] & 0xFFU) >> 4;
            unsigned int p3 = (src[x + 3] & 0xFFU) >> 4;
            dst[x / 4] = (unsigned short)(p0 | (p1 << 4) | (p2 << 8) | (p3 << 12));
        }
    }

    if (old_font != NULL) {
        SelectObject(dc, old_font);
    }
    if (font != NULL) {
        DeleteObject(font);
    }
    SelectObject(dc, old_bmp);
    DeleteObject(bmp);
    DeleteDC(dc);
    return 1;
}

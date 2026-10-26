#include "enhance/pause_menu/assets.h"
#include "enhance/pause_menu/menu.h"

/* Execute the game's actual frame geometry with private canvas sinks. This
 * preserves its bevel, corners and gradient; no replacement UI art is authored. */
#define window_frame_draw hm_pause_original_frame
#define prim_add_pixel hm_pause_pixel
#define prim_add_line hm_pause_line
#define prim_add_gouraud_rect hm_pause_gradient
#include "../../../src/main/ui/window_frame.c"
#undef window_frame_draw
#undef prim_add_pixel
#undef prim_add_line
#undef prim_add_gouraud_rect

static u8 *hm_canvas;
static void hm_put(int x, int y, u32 color) {
    if (x < 0 || y < 0 || x >= 320 || y >= 240)
        return;
    u8 *p = hm_canvas + ((size_t)y * 320 + (size_t)x) * 4;
    /* Original prim_make_pixel/line/gouraud_rect consume 0xRRGGBB. */
    p[0] = (u8)(color >> 16);
    p[1] = (u8)(color >> 8);
    p[2] = (u8)color;
    p[3] = 255;
}
void hm_pause_pixel(u_long *ot, s16 x, s16 y, s32 color, s16 semi) {
    (void)ot;
    (void)semi;
    hm_put(x, y, (u32)color);
}
void hm_pause_line(u_long *ot, s16 x0, s16 y0, s16 x1, s16 y1, s32 color, s16 semi) {
    (void)ot;
    (void)semi;
    /* The original window frame uses only horizontal/vertical edges. */
    if (x0 == x1)
        for (int y = y0; y <= y1; ++y)
            hm_put(x0, y, (u32)color);
    else if (y0 == y1)
        for (int x = x0; x <= x1; ++x)
            hm_put(x, y0, (u32)color);
}
void hm_pause_gradient(u_long *ot, s16 x, s16 y, s16 w, s16 h, s32 c0, s32 c1, s32 c2, s32 c3,
                       s16 semi) {
    (void)ot;
    (void)c1;
    (void)c3;
    (void)semi;
    for (int row = 0; row < h; ++row) {
        u32 color = 0;
        for (unsigned channel = 0; channel < 3; ++channel) {
            int top = ((u32)c0 >> (channel * 8)) & 255;
            int bottom = ((u32)c2 >> (channel * 8)) & 255;
            color |= (u32)(top + (bottom - top) * row / (h > 1 ? h - 1 : 1)) << (channel * 8);
        }
        for (int col = 0; col < w; ++col)
            hm_put(x + col, y + row, color);
    }
}
static void hm_text(const hm_pause_assets *assets, const char *text, int x, int y) {
    if (!text)
        return;
    for (; *text && x < 304; ++text, x += 8) {
        unsigned char c = (unsigned char)*text;
        unsigned glyph;
        if (c >= '0' && c <= '9')
            glyph = 0x40 + c - '0';
        else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            glyph = c - 'A';
        else if (c == '-')
            glyph = 0x69;
        else if (c == '(')
            glyph = 0x6A;
        else if (c == ')')
            glyph = 0x6B;
        else
            continue;
        for (unsigned row = 0; row < 12; ++row)
            for (unsigned col = 0; col < 8; ++col) {
                u16 color = assets->font[((glyph / 16) * 12 + row) * HM_PAUSE_FONT_W +
                                         (glyph % 16) * 8 + col];
                if (!color)
                    continue;
                u32 rgb = 0;
                for (unsigned k = 0; k < 3; ++k) {
                    unsigned value = (color >> (k * 5)) & 31;
                    rgb |= ((value << 3) | (value >> 2)) << ((2 - k) * 8);
                }
                hm_put(x + (int)col, y + (int)row, rgb);
            }
    }
}
static void hm_center(const hm_pause_assets *assets, const char *text, int y) {
    if (!text)
        return;
    size_t length = strlen(text);
    if (length > 36)
        length = 36;
    hm_text(assets, text, 160 - (int)length * 4, y);
}
void hm_pause_draw(const hm_pause_assets *assets, u8 *rgba, const hm_pause_menu *menu,
                   const hm_pause_view *view) {
    hm_canvas = rgba;
    if (menu->page == HM_PAUSE_GRAPHICS) {
        hm_pause_original_frame(NULL, 12, 8, 296, 28);
        hm_center(assets, "Settings - Graphics", 16);
        hm_pause_original_frame(NULL, 12, 40, 296, 156);
        hm_pause_original_frame(NULL, 20, 45 + menu->selected * 20, 280, 20);
        for (int i = 0; i < hm_pause_count(menu); ++i) {
            int y = 49 + i * 20;
            if (i == menu->selected)
                hm_text(assets, "-", 26, y);
            hm_text(assets, hm_pause_label(menu, i), 40, y);
            if (view && view->values[i])
                hm_text(assets, view->values[i], 192, y);
        }
        hm_pause_original_frame(NULL, 12, 200, 296, 32);
        hm_text(assets, view ? view->detail : NULL, 24, 205);
        hm_text(assets,
                view && view->notice && *view->notice ? view->notice : "Left Right Change   D Back",
                24, 218);
    } else {
        int root = menu->page == HM_PAUSE_ROOT;
        int top = root ? 54 : 72;
        hm_pause_original_frame(NULL, 68, top, 184, root ? 132 : 96);
        hm_center(assets, root ? "Paused" : "Settings", top + 12);
        hm_pause_line(NULL, 84, top + 32, 236, top + 32, FRAME_DARK, 0);
        int row_start = top + 44;
        hm_pause_original_frame(NULL, 80, row_start + menu->selected * 24 - 4, 160, 22);
        for (int i = 0; i < hm_pause_count(menu); ++i) {
            int y = row_start + i * 24;
            if (i == menu->selected)
                hm_text(assets, "-", 90, y);
            hm_text(assets, hm_pause_label(menu, i), 108, y);
        }
        hm_center(assets, "X Confirm   D Back", 204);
        hm_center(assets, root ? "ESC Resume" : "ESC Back", 220);
    }
    hm_canvas = NULL;
}

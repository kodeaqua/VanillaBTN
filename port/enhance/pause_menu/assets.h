#ifndef HMBTN_PAUSE_ASSETS_H
#define HMBTN_PAUSE_ASSETS_H
#include "enhance/pause_menu/menu.h"
#include "psx/disc.h"

enum { HM_PAUSE_FONT_W = 128, HM_PAUSE_FONT_H = 120 };
typedef struct {
    u16 font[HM_PAUSE_FONT_W * HM_PAUSE_FONT_H];
} hm_pause_assets;
typedef struct {
    const char *values[7];
    const char *detail;
    const char *notice;
} hm_pause_view;
int hm_pause_assets_load(hm_pause_assets *assets, hm_disc *disc);
/* RGBA canvas is always the original logical 320x240; never touches game VRAM. */
void hm_pause_draw(const hm_pause_assets *assets, u8 *rgba, const hm_pause_menu *menu,
                   const hm_pause_view *view);
#endif

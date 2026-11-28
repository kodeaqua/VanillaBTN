#ifndef UI_ICONS_H
#define UI_ICONS_H


/* Numbered icons of the UI sprite sheet. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>
#include "types.h"
#include "anim.h"

void ui_draw_icon_2a(void *ot, s16 x, s16 y);
void ui_draw_icon_27_0(void *ot, s16 x, s16 y);
void ui_draw_icon_27_2(void *ot, s16 x, s16 y);
void ui_draw_icon_27_1(void *ot, s16 x, s16 y);
void ui_draw_icon_27_3(void *ot, s16 x, s16 y);
void ui_blink_tick(s16 advance);
void ui_draw_icon_29_phase(void *ot, s16 x, s16 y);
void ui_draw_icon_28_phase(void *ot, s16 x, s16 y);
void ui_draw_icon_0d_phase(void *ot, s16 x, s16 y);
void ui_draw_icon_0c_phase(void *ot, s16 x, s16 y);
void ui_draw_icon_0b_1(void *ot, s16 x, s16 y);
void ui_draw_icon_18_sub(void *ot, s16 x, s16 y, s16 sub);
void ui_draw_icon_1e(void *ot, s16 x, s16 y);
void ui_blink_tick_60(s16 advance);
void ui_draw_icon_07_phase(void *ot, s16 x, s16 y);
void ui_draw_icon_2b_sub(void *ot, s16 x, s16 y, s16 sub);

/* The shared sprite every "draw one item" path stages into. */
extern AnimState ui_icon_anim;
#ifdef __psyz
void ui_icons_bind_stage(void);
#endif

#endif /* UI_ICONS_H */

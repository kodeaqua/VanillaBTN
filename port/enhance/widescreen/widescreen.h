#ifndef HMBTN_WIDESCREEN_H
#define HMBTN_WIDESCREEN_H
#include <libgpu.h>
/* Presentation-only horizontal field of view; native coordinates stay intact. */
void hm_widescreen_configure(int enabled, int width, int height);
void hm_widescreen_scene(int world);
int hm_widescreen_margin(void);
int hm_widescreen_angle(int angle);
int hm_widescreen_radius(int radius);
RECT hm_widescreen_area(RECT native);
void hm_widescreen_replay_begin(const RECT *native);
void hm_widescreen_replay_group(int world);
RECT hm_widescreen_clip(RECT clip);
int hm_widescreen_overlay(const u32 *words, u32 count, u32 output[5]);
#endif

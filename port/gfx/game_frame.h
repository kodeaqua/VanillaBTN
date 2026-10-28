#ifndef HMBTN_GFX_GAME_FRAME_H
#define HMBTN_GFX_GAME_FRAME_H
#include "gfx/frame_events.h"
hm_render_frame hm_game_render_frame(void);
/* Bytes available from the current frame's primitive start (a private wide arena when active). */
size_t hm_game_prim_capacity(void);
#endif

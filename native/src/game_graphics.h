#ifndef VANILLABTN_GAME_GRAPHICS_H
#define VANILLABTN_GAME_GRAPHICS_H

#include "ps1_video.h"

/* Native readers for the game's 0x0D texture bank and 0x0E animation formats.
 * These retain serialized 32-bit offsets; no asset bytes become host pointers. */
bool game_texture_pack_upload(Ps1Video *video, const uint8_t *data, size_t size);
bool game_animation_draw(Ps1Video *video, const Ps1DrawState *draw,
                          const uint8_t *data, size_t size, unsigned animation,
                          unsigned frame, int x, int y);

typedef struct GameAnimation {
    unsigned animation, frame, ticks;
} GameAnimation;

bool game_animation_tick(GameAnimation *state, const uint8_t *data, size_t size);
bool game_animation_draw_quads(Ps1Video *video, const Ps1DrawState *draw,
                                const uint8_t *data, size_t size, unsigned animation,
                                unsigned frame, int x, int y);

#endif

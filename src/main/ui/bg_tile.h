#ifndef BG_TILE_H
#define BG_TILE_H


/* Tiling the background with a sprite or a cursor. */
#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>
#include <libgs.h>

void bg_tile_mode_sprite(void *arg);
void bg_tile_mode_cursor(void *arg);

#endif /* BG_TILE_H */

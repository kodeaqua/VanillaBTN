/* Native indirect calls require an exact TileDrawFn signature. Keep the
 * original draw functions and their signed-short coordinate conversions. */
static void hm_bg_tile_sprite(u_long *ot, AnimState *sprite, s16 frame, s32 x, s32 y) {
    bg_tile_draw_sprite(ot, sprite, frame, (s16)x, (s16)y);
}
static void hm_bg_tile_cursor(u_long *ot, AnimState *sprite, s16 frame, s32 x, s32 y) {
    bg_tile_draw_cursor(ot, sprite, frame, (s16)x, (s16)y);
}

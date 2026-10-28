/* Original PS1 slots 3/4 address the two adjacent shared sprite globals.
 * Host linkers need not place them after the three-element page array. */
static AnimState *hm_status_sprite(s32 slot) {
    if (slot >= 0 && slot < 3)
        return &status_page_sprites[slot];
    if (slot == 3)
        return &status_cursor_sprite;
    if (slot == 4)
        return &status_mark_sprite;
    return &status_cursor_sprite;
}

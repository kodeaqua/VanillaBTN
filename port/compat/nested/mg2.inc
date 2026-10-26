/* Native copy of the repository's CC0 nested helper. No algorithm changes. */
static void mg2_draw_board_marker(s32 bank, s32 frame) {
    anim_select_range(&mg2_state.iconSprite, bank & 0xFF);
    anim_set_frame(&mg2_state.iconSprite, frame);
    anim_draw_sprt_cursor(&current_frame->otTags[0][5], &mg2_state.iconSprite,
                          mg2_state.iconSprite.animation, mg2_state.iconSprite.frame, 0, 0);
}

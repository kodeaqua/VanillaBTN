/* Native copy of the repository's CC0 nested helper. No algorithm changes. */
static void swind_screen_area_copy(s32 restore) {
    u_long *buffer = (u_long *)STAGING_BUFFER;

    if (restore == 0) {
        StoreImage(&swind_save_rect, buffer);
    } else {
        LoadImage(&swind_save_rect, buffer);
    }
    DrawSync(0);
}

/* Native copies of the repository's CC0 nested helpers. No algorithm changes. */
static s32 has_medal_price(u8 *flags) {
    s32 j;

    for (j = 0; j < 10; j++, flags++) {
        if (*flags == 2) {
            return 1;
        }
    }
    return 0;
}

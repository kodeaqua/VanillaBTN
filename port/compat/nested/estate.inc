/* Native copies of the repository's CC0 nested helpers. No algorithm changes. */
static s32 estate_kind_value_for_flags(u16 flags, s16 valueWhenBit13, s16 valueWhenBit11,
                                       s16 valueWhenBit12) {
    if (flags & 0x2000)
        return valueWhenBit13;
    if (flags & 0x800)
        return valueWhenBit11;
    if (flags & 0x1000)
        return valueWhenBit12;
    return 0;
}

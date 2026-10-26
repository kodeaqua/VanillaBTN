/* Native copies of the repository's CC0 nested helpers. No algorithm changes. */
static s32 farm_kind_matches_flags(FarmActor *object, u32 kind) {
    u32 flags;
    u32 diedOfAge;

    flags = object->flags;
    diedOfAge = flags & ACTOR_FLAG_DIED_OF_AGE;
    if (flags & ACTOR_FLAG_DIED) {
        if ((kind & 1) && diedOfAge == 1) {
            return 1;
        }
        if ((kind & 2) && diedOfAge == 0) {
            return 1;
        }
    }
    return 0;
}

static s32 farm_move_compare(s32 a, s32 b) {
    if (a == b) {
        return 0;
    }
    if (a < b) {
        return 1;
    }
    return 2;
}

static s32 farm_heading_compare(s32 a, s32 b) {
    if (a == b) {
        return 0;
    }
    if (a < b) {
        return 1;
    }
    return 2;
}

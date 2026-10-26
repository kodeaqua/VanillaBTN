/* Native copies of the repository's CC0 nested helpers. No algorithm changes. */
static s32 winter_23_single_check(s32 which) {
    if (game_state.progress.clock.date.season == SEASON_WINTER &&
        game_state.progress.clock.date.day == 23 && event_flag_get(EVENT_FLAG_SINGLE) == 1 &&
        event_flag_get(EVENT_FLAG_KAREN_RICK_MARRIED + which) == 0) {
        return 1;
    }
    return 0;
}

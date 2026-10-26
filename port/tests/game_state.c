#include "event_flags.h"
#include "farm_state.h"
#include "farm_tables.h"

#include <stdio.h>

/* Exercise host-side logic only. This object is never a disc/save payload. */
FarmState game_state;

#define CHECK(condition)                                                                            \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);                        \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void) {
    const u16 flag_count = sizeof(game_state.eventFlags) / sizeof(u32) * 32;
    s32 records = 0;

    /* Include bit 31, word boundaries and the last valid bit. No out-of-range IDs. */
    for (u16 flag = 0; flag < flag_count; ++flag) {
        CHECK(event_flag_get(flag) == 0);
        event_flag_set(flag, 7);
        CHECK(event_flag_get(flag) == 1);
        if (flag > 0) {
            CHECK(event_flag_get(flag - 1) == 0);
        }
        event_flag_set(flag, 0);
        CHECK(event_flag_get(flag) == 0);
    }
    event_flag_set(31, 1);
    event_flag_set(32, 1);
    event_flag_set(31, 0);
    CHECK(event_flag_get(32) == 1);

    farm_tables_register(0);
    CHECK(game_state.itemInfo != NULL && game_state.pictureGrids != NULL);
    CHECK(game_state.tileRules != NULL && game_state.cropTable != NULL);
    CHECK(game_state.pictureCodes != NULL);
    for (s32 run = 0; run < 19; ++run) {
        CHECK(game_state.runs.counts[run] > 0);
        CHECK(game_state.runs.records[run] == game_state.runs.records[0] + records);
        records += game_state.runs.counts[run];
    }
    CHECK(records == ESTATE_RECORD_COUNT);
    game_state.runs.counts[0] = -1;
    farm_tables_register(1);
    CHECK(game_state.runs.counts[0] == -1); /* skipRuns preserves existing run state. */
    game_state.progress.entryStep = 5;
    game_state.progress.entrySubStep = 7;
    farm_entry_args_clear();
    CHECK(game_state.progress.entryStep == 0 && game_state.progress.entrySubStep == 0);
    game_state.system.workSectorBusy = 0;
    CHECK(farm_work_sector_get() == game_state.work.sector);
    game_state.system.workSectorBusy = 1;
    CHECK(farm_work_sector_get() == NULL);
    farm_work_sector_release();
    CHECK(farm_work_sector_get() == game_state.work.sector);
    return 0;
}

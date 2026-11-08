/*
 * ps1_input_test: synthetic checks for pad.c controller emulation and repeat semantics.
 */
#include "ps1_input.h"
#include <assert.h>

int main(void) {
    assert(ps1_input_stick_buttons(0, 0) == 0);
    assert(ps1_input_stick_buttons(0, -32768) == PS1_PAD_UP);
    assert(ps1_input_stick_buttons(32767, 0) == PS1_PAD_RIGHT);
    assert(ps1_input_stick_buttons(0, 32767) == PS1_PAD_DOWN);
    assert(ps1_input_stick_buttons(-32768, 0) == PS1_PAD_LEFT);
    Ps1InputRecord record;
    ps1_input_init(&record);

    if (record.delay != 20 || record.rate != 7) return 1;
    if (record.held != 0 || record.pressed != 0 || record.triggered != 0) return 2;

    /* 1. Single frame press and release */
    ps1_input_set_button(&record, PS1_PAD_CROSS, true);
    ps1_input_tick(&record);

    if (record.held != PS1_PAD_CROSS) return 3;
    if (record.pressed != PS1_PAD_CROSS) return 4;
    if (record.triggered != PS1_PAD_CROSS) return 5;

    /* Frame 2: held without change */
    ps1_input_tick(&record);
    if (record.held != PS1_PAD_CROSS) return 6;
    if (record.pressed != 0) return 7;
    if (record.triggered != 0) return 8;

    /* Frame 3: release */
    ps1_input_set_button(&record, PS1_PAD_CROSS, false);
    ps1_input_tick(&record);
    if (record.held != 0 || record.pressed != 0 || record.triggered != 0) return 9;

    /* 2. Auto-repeat cadence: delay = 3 frames, rate = 2 frames */
    ps1_input_clear(&record);
    ps1_input_set_repeat(&record, 3, 2);

    ps1_input_set_button(&record, PS1_PAD_UP, true);
    ps1_input_tick(&record); /* frame 1: initial press */
    if (record.triggered != PS1_PAD_UP) return 10;

    ps1_input_tick(&record); /* frame 2: repeat_frames = 1 */
    if (record.triggered != 0) return 11;

    ps1_input_tick(&record); /* frame 3: repeat_frames = 2 */
    if (record.triggered != 0) return 12;

    ps1_input_tick(&record); /* frame 4: repeat_frames = 3 */
    if (record.triggered != 0) return 13;

    ps1_input_tick(&record); /* frame 5: repeat_frames = 4 > delay(3) -> repeating = 1 */
    if (record.triggered != 0) return 14;
    if (record.repeating != 1) return 15;

    ps1_input_tick(&record); /* frame 6: repeat_frames = 1 */
    if (record.triggered != 0) return 16;

    ps1_input_tick(&record); /* frame 7: repeat_frames = 2 % rate(2) == 0 -> triggered */
    if (record.triggered != PS1_PAD_UP) return 17;

    ps1_input_tick(&record); /* frame 8: repeat_frames = 3 */
    if (record.triggered != 0) return 18;

    ps1_input_tick(&record); /* frame 9: repeat_frames = 4 % rate(2) == 0 -> triggered */
    if (record.triggered != PS1_PAD_UP) return 19;

    /* 3. Dual repeat channel independence */
    ps1_input_clear(&record);
    ps1_input_set_repeat(&record, 2, 2);
    ps1_input_set_repeat2(&record, 4, 3);

    ps1_input_set_button(&record, PS1_PAD_START, true);
    ps1_input_tick(&record); /* frame 1 */
    if (record.triggered != PS1_PAD_START || record.triggered2 != PS1_PAD_START) return 20;

    ps1_input_tick(&record); /* frame 2: repeat_frames = 1 */
    ps1_input_tick(&record); /* frame 3: repeat_frames = 2 */
    ps1_input_tick(&record); /* frame 4: repeat_frames = 3 > delay(2) -> channel 1 repeating = 1 */
    if (record.repeating != 1 || record.repeating2 != 0) return 21;

    /* 4. Multi-button chord */
    ps1_input_clear(&record);
    ps1_input_set_button(&record, PS1_PAD_L1 | PS1_PAD_TRIANGLE, true);
    ps1_input_tick(&record);
    if (record.held != (PS1_PAD_L1 | PS1_PAD_TRIANGLE)) return 22;
    if (record.pressed != (PS1_PAD_L1 | PS1_PAD_TRIANGLE)) return 23;

    /* FAST_2 scales both channels independently, as main_run_frame requires. */
    ps1_input_init(&record);
    ps1_input_set_repeat(&record, 6, 4);
    ps1_input_set_repeat2(&record, 2, 2);
    record.held = PS1_PAD_UP;
    ps1_input_tick_speed(&record, 2);
    for (unsigned i = 0; i < 4; ++i) ps1_input_tick_speed(&record, 2);
    assert(record.repeating == 1 && record.triggered == 0);
    assert(record.triggered2 == PS1_PAD_UP);
    ps1_input_tick_speed(&record, 2);
    assert(record.triggered == 0);
    ps1_input_tick_speed(&record, 2);
    assert(record.triggered == PS1_PAD_UP);
    return 0;
}

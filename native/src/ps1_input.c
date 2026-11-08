#include "ps1_input.h"

#include <string.h>

void ps1_input_init(Ps1InputRecord *record) {
    if (record == NULL) return;
    memset(record, 0, sizeof(*record));
    record->delay = PS1_PAD_DEFAULT_REPEAT_DELAY;
    record->rate  = PS1_PAD_DEFAULT_REPEAT_RATE;
    record->delay2 = PS1_PAD_DEFAULT_REPEAT_DELAY;
    record->rate2  = PS1_PAD_DEFAULT_REPEAT_RATE;
}

void ps1_input_set_repeat(Ps1InputRecord *record, int32_t delay, int32_t rate) {
    if (record == NULL) return;
    record->delay = delay;
    record->rate  = rate <= 0 ? 1 : rate;
}

void ps1_input_set_repeat2(Ps1InputRecord *record, int32_t delay2, int32_t rate2) {
    if (record == NULL) return;
    record->delay2 = delay2;
    record->rate2  = rate2 <= 0 ? 1 : rate2;
}

void ps1_input_set_button(Ps1InputRecord *record, uint32_t button, bool pressed) {
    if (record == NULL) return;
    if (pressed) {
        record->held |= button;
    } else {
        record->held &= ~button;
    }
}

void ps1_input_tick(Ps1InputRecord *record) {
    if (record == NULL) return;

    record->pressed = (record->previous ^ record->held) & record->held;

    if (record->held == record->previous) {
        int32_t rate = record->rate <= 0 ? 1 : record->rate;
        if (record->repeating == 0) {
            record->triggered = 0;
            record->repeat_frames++;
            if ((int32_t)record->repeat_frames > record->delay) {
                record->repeat_frames = 0;
                record->repeating = 1;
            }
        } else {
            record->repeat_frames++;
            if (record->repeat_frames % (uint32_t)rate == 0) {
                record->triggered = record->held;
            } else {
                record->triggered = 0;
            }
        }

        int32_t rate2 = record->rate2 <= 0 ? 1 : record->rate2;
        if (record->repeating2 == 0) {
            record->triggered2 = 0;
            record->repeat_frames2++;
            if ((int32_t)record->repeat_frames2 > record->delay2) {
                record->repeat_frames2 = 0;
                record->repeating2 = 1;
            }
        } else {
            record->repeat_frames2++;
            if (record->repeat_frames2 % (uint32_t)rate2 == 0) {
                record->triggered2 = record->held;
            } else {
                record->triggered2 = 0;
            }
        }
    } else {
        record->triggered = record->pressed;
        record->triggered2 = record->pressed;
        record->repeat_frames = 0;
        record->repeating = 0;
        record->repeat_frames2 = 0;
        record->repeating2 = 0;
    }

    record->previous = record->held;
}

void ps1_input_clear(Ps1InputRecord *record) {
    if (record == NULL) return;
    record->held = 0;
    record->pressed = 0;
    record->previous = 0;
    record->repeat_frames = 0;
    record->repeating = 0;
    record->repeat_frames2 = 0;
    record->repeating2 = 0;
    record->triggered = 0;
    record->triggered2 = 0;
}

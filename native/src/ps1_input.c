#include "ps1_input.h"
#include <math.h>
#include <stdlib.h>

uint16_t ps1_input_keyboard_event(uint16_t *keys, size_t count, size_t key,
                                  uint16_t button, bool down) {
    if (keys == NULL || count == 0) return 0;
    uint16_t buttons = 0;
    if (key < count) keys[key] = down ? button : 0;
    for (size_t i = 0; i < count; ++i) buttons |= keys[i];
    return buttons;
}

uint32_t ps1_input_trigger_buttons(int16_t left, int16_t right) {
    return (left >= 16384 ? PS1_PAD_L2 : 0)
        | (right >= 16384 ? PS1_PAD_R2 : 0);
}

uint32_t ps1_input_stick_buttons(int16_t host_x, int16_t host_y) {
    int x = ((int)host_x + 32768) / 256;
    int y = ((int)host_y + 32768) / 256;
    int angle;
    if (x >= 92 && x <= 164 && y >= 92 && y <= 164) return 0;
    int dx = abs(x - 128), dy = abs(y - 128);
    if (dy == 0) angle = 90;
    else if (dx == 0) angle = 0;
    else angle = (int)(atan2((double)dx, (double)dy) * 4096.0 / 6.283185307179586) * 360 / 4096;
    if (x >= 128) { if (y > 128) angle = 180 - angle; }
    else if (y > 128) angle += 180;
    else angle = 360 - angle;
    if (angle == 360) angle = 0;
    if (angle < 45 || angle >= 315) return PS1_PAD_UP;
    if (angle < 135) return PS1_PAD_RIGHT;
    if (angle < 225) return PS1_PAD_DOWN;
    return PS1_PAD_LEFT;
}

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
    ps1_input_tick_speed(record, 1);
}

void ps1_input_tick_speed(Ps1InputRecord *record, unsigned speed_mode) {
    if (record == NULL) return;
    int divisor = speed_mode == 2 || speed_mode == 3 ? (int)speed_mode : 1;

    record->pressed = (record->previous ^ record->held) & record->held;

    if (record->held == record->previous) {
        int32_t rate = record->rate / divisor;
        if (rate <= 0) rate = 1;
        if (record->repeating == 0) {
            record->triggered = 0;
            record->repeat_frames++;
            if ((int32_t)record->repeat_frames > record->delay / divisor) {
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

        int32_t rate2 = record->rate2 / divisor;
        if (rate2 <= 0) rate2 = 1;
        if (record->repeating2 == 0) {
            record->triggered2 = 0;
            record->repeat_frames2++;
            if ((int32_t)record->repeat_frames2 > record->delay2 / divisor) {
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

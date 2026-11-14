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

void ps1_input_normalize_stick(int16_t host_x, int16_t host_y, float deadzone,
                              bool fix_diagonal_running,
                              uint8_t *out_x, uint8_t *out_y) {
    if (out_x == NULL || out_y == NULL) return;

    if (!fix_diagonal_running) {
        /* Authentic PS1 linear truncation without circular rescaling */
        *out_x = (uint8_t)(((int)host_x + 32768) / 256);
        *out_y = (uint8_t)(((int)host_y + 32768) / 256);
        return;
    }

    /* Convert to normalized floating-point range [-1.0, 1.0] */
    float nx = (float)host_x / 32767.0f;
    float ny = (float)host_y / 32767.0f;
    if (nx < -1.0f) nx = -1.0f; if (nx > 1.0f) nx = 1.0f;
    if (ny < -1.0f) ny = -1.0f; if (ny > 1.0f) ny = 1.0f;

    float r = sqrtf(nx * nx + ny * ny);

    /* Public callers may supply a nonfinite deadzone even though the INI
     * parser rejects one. Normalize it before arithmetic and int casts. */
    if (!isfinite(deadzone) || deadzone < 0.0f) deadzone = 0.0f;
    if (deadzone > 0.9f) deadzone = 0.9f;

    if (r <= deadzone) {
        *out_x = 128;
        *out_y = 128;
        return;
    }

    /* Unit direction vector */
    float dir_x = nx / r;
    float dir_y = ny / r;

    /* Rescale radial magnitude from [deadzone, 1.0] to [0.0, 1.0] */
    float r_norm = (r - deadzone) / (1.0f - deadzone);
    if (r_norm > 1.0f) r_norm = 1.0f;

    /* Circular-to-octagonal gate scaling:
     * In an octagonal/square gate, diagonal corners reach distance sqrt(2) ~ 1.414.
     * Scale by 1.0 / max(|dir_x|, |dir_y|) so diagonal pushes reach full 1.0 magnitude. */
    float max_axis = fmaxf(fabsf(dir_x), fabsf(dir_y));
    float scale = (max_axis > 1e-4f) ? (1.0f / max_axis) : 1.0f;

    float out_nx = dir_x * r_norm * scale;
    float out_ny = dir_y * r_norm * scale;

    if (out_nx < -1.0f) out_nx = -1.0f; if (out_nx > 1.0f) out_nx = 1.0f;
    if (out_ny < -1.0f) out_ny = -1.0f; if (out_ny > 1.0f) out_ny = 1.0f;

    int final_x = (int)roundf(128.0f + out_nx * 127.0f);
    int final_y = (int)roundf(128.0f + out_ny * 127.0f);

    if (final_x < 0) final_x = 0; if (final_x > 255) final_x = 255;
    if (final_y < 0) final_y = 0; if (final_y > 255) final_y = 255;

    *out_x = (uint8_t)final_x;
    *out_y = (uint8_t)final_y;
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

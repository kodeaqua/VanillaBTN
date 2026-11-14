#ifndef VANILLABTN_PS1_INPUT_H
#define VANILLABTN_PS1_INPUT_H

/*
 * ps1_input: PS1 controller port emulation and host input mapping.
 *
 * Replaces PSY-Q's libpad and pad.c hardware polling with host-side keyboard
 * and gamepad mapping.  Implements the exact InputRecord semantics from pad.c:
 *   - held: bitmask of buttons currently pressed
 *   - pressed: newly pressed this frame (held & ~previous)
 *   - triggered / triggered2: dual auto-repeat channels with configurable
 *     delay and rate
 *
 * Button bit values match enum PadButton from src/main/input/pad.h.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PS1_PAD_L2       0x0001u
#define PS1_PAD_R2       0x0002u
#define PS1_PAD_L1       0x0004u
#define PS1_PAD_R1       0x0008u
#define PS1_PAD_TRIANGLE 0x0010u
#define PS1_PAD_CIRCLE   0x0020u
#define PS1_PAD_CROSS    0x0040u
#define PS1_PAD_SQUARE   0x0080u
#define PS1_PAD_SELECT   0x0100u
#define PS1_PAD_START    0x0800u
#define PS1_PAD_UP       0x1000u
#define PS1_PAD_RIGHT    0x2000u
#define PS1_PAD_DOWN     0x4000u
#define PS1_PAD_LEFT     0x8000u

#define PS1_PAD_ARROWS_MASK (PS1_PAD_UP | PS1_PAD_RIGHT | PS1_PAD_DOWN | PS1_PAD_LEFT)

#define PS1_PAD_DEFAULT_REPEAT_DELAY 20
#define PS1_PAD_DEFAULT_REPEAT_RATE  7

typedef struct Ps1InputRecord {
    uint32_t held;
    uint32_t pressed;
    uint32_t previous;
    uint32_t triggered;
    uint32_t repeat_frames;
    int32_t repeating;
    int32_t delay;
    int32_t rate;

    uint32_t triggered2;
    uint32_t repeat_frames2;
    int32_t repeating2;
    int32_t delay2;
    int32_t rate2;
} Ps1InputRecord;

void ps1_input_init(Ps1InputRecord *record);
void ps1_input_set_repeat(Ps1InputRecord *record, int32_t delay, int32_t rate);
void ps1_input_set_repeat2(Ps1InputRecord *record, int32_t delay2, int32_t rate2);

/* Set or clear an individual button in the held state. */
void ps1_input_set_button(Ps1InputRecord *record, uint32_t button, bool pressed);

/* Advance one simulation tick: computes pressed and updates auto-repeat channels. */
void ps1_input_tick(Ps1InputRecord *record);
/* pad.c divides repeat delay/rate by FAST_2/FAST_3 (2/3) at lower frame rates. */
void ps1_input_tick_speed(Ps1InputRecord *record, unsigned speed_mode);

/* Clear all held, pressed, and repeat states. */
void ps1_input_clear(Ps1InputRecord *record);

/* Fold an SDL signed left stick into pad.c's four cardinal sectors/dead zone. */
uint32_t ps1_input_stick_buttons(int16_t x, int16_t y);

/* Normalize raw 16-bit signed analog stick into 8-bit PS1 [0..255] stick range with
 * circular gate normalization (fixing the diagonal running walk-drop bug) and deadzone. */
void ps1_input_normalize_stick(int16_t host_x, int16_t host_y, float deadzone,
                              bool fix_diagonal_running,
                              uint8_t *out_x, uint8_t *out_y);

/* Convert SDL trigger axes into the original digital L2/R2 buttons. */
uint32_t ps1_input_trigger_buttons(int16_t left, int16_t right);

/* Keep keyboard aliases independent until their physical keys are released. */
uint16_t ps1_input_keyboard_event(uint16_t *keys, size_t count, size_t key,
                                  uint16_t button, bool down);

#endif /* VANILLABTN_PS1_INPUT_H */

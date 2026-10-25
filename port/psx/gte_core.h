#ifndef HMBTN_PSX_GTE_CORE_H
#define HMBTN_PSX_GTE_CORE_H

#include "compat/types.h"
#include "psx/error.h"

/* Scalar reference for the source-used arithmetic commands. This is hardware
 * API state in native C, not a CPU interpreter or an emulated RAM image. */
typedef struct {
    s16 vector[3][3];
    s16 matrix[3][3][3];
    s32 translation[3][3];
    s16 ir[3], sx[3], sy[3];
    u16 sz[4], otz;
    s16 zsf[2];
    s32 mac[4];
    u32 flags;
} hm_gte_state;

/* Explicit arithmetic shift and wrapping avoid signed shifts/overflow UB. */
static inline s64 hm_gte_shift(s64 value, u32 bits) {
    if (!bits)
        return value;
    if (value >= 0)
        return value / (INT64_C(1) << bits);
    return -1 - ((-1 - value) / (INT64_C(1) << bits));
}

static inline s32 hm_gte_word(s64 value) {
    u32 low = (u32)(u64)value;
    return low <= INT32_MAX ? (s32)low : -1 - (s32)(UINT32_MAX - low);
}

static inline s64 hm_gte_accumulate(hm_gte_state *state, u32 axis, s64 value) {
    const s64 limit = INT64_C(1) << 43;
    if (value >= limit)
        state->flags |= UINT32_C(1) << (30 - axis);
    if (value < -limit)
        state->flags |= UINT32_C(1) << (27 - axis);
    u64 low = (u64)value & ((UINT64_C(1) << 44) - 1);
    return low < (UINT64_C(1) << 43) ? (s64)low : (s64)low - (INT64_C(1) << 44);
}

static inline void hm_gte_ir(hm_gte_state *state, u32 axis, s32 value, int positive_only) {
    s32 minimum = positive_only ? 0 : -32768;
    if (value < minimum || value > 32767)
        state->flags |= UINT32_C(1) << (24 - axis);
    state->ir[axis] = (s16)(value < minimum ? minimum : value > 32767 ? 32767 : value);
}

static inline void hm_gte_mac(hm_gte_state *state, u32 axis, s64 value, u32 shift, int lm) {
    value = hm_gte_accumulate(state, axis, value);
    state->mac[axis + 1] = hm_gte_word(hm_gte_shift(value, shift));
    hm_gte_ir(state, axis, state->mac[axis + 1], lm);
}

static inline void hm_gte_mac0(hm_gte_state *state, s64 value) {
    if (value > INT32_MAX)
        state->flags |= UINT32_C(1) << 16;
    if (value < INT32_MIN)
        state->flags |= UINT32_C(1) << 15;
    state->mac[0] = hm_gte_word(value);
}

static inline void hm_gte_summary(hm_gte_state *state) {
    if (state->flags & UINT32_C(0x7F87E000))
        state->flags |= UINT32_C(0x80000000);
}

static inline void hm_gte_square(hm_gte_state *state, u32 shift, int lm) {
    s16 input[3] = {state->ir[0], state->ir[1], state->ir[2]};
    for (u32 i = 0; i < 3; ++i)
        hm_gte_mac(state, i, (s64)input[i] * input[i], shift, lm);
}

static inline void hm_gte_cross(hm_gte_state *state, u32 shift, int lm) {
    s16 input[3] = {state->ir[0], state->ir[1], state->ir[2]};
    s16 diagonal[3] = {state->matrix[0][0][0], state->matrix[0][1][1], state->matrix[0][2][2]};
    for (u32 i = 0; i < 3; ++i) {
        u32 j = (i + 1) % 3, k = (i + 2) % 3;
        hm_gte_mac(state, i, (s64)diagonal[j] * input[k] - (s64)diagonal[k] * input[j], shift, lm);
    }
}

static inline void hm_gte_multiply(hm_gte_state *state, u32 matrix, u32 vector, u32 translation,
                                   u32 shift, int lm) {
    if (matrix == 3 || translation == 2)
        hm_runtime_error("hm_gte_multiply", "unused hardware-bug selector is not implemented");
    s16 input[3];
    for (u32 i = 0; i < 3; ++i)
        input[i] = vector == 3 ? state->ir[i] : state->vector[vector][i];
    for (u32 row = 0; row < 3; ++row) {
        s64 sum = translation == 3 ? 0 : (s64)state->translation[translation][row] * 4096;
        for (u32 col = 0; col < 3; ++col)
            sum = hm_gte_accumulate(state, row,
                                    sum + (s64)state->matrix[matrix][row][col] * input[col]);
        hm_gte_mac(state, row, sum, shift, lm);
    }
}

static inline void hm_gte_clip(hm_gte_state *state) {
    s64 area = 0;
    for (u32 i = 0; i < 3; ++i) {
        u32 next = (i + 1) % 3;
        area += (s64)state->sx[i] * state->sy[next] - (s64)state->sx[next] * state->sy[i];
    }
    hm_gte_mac0(state, area);
}

static inline void hm_gte_average(hm_gte_state *state, int four) {
    u32 sum = four ? state->sz[0] : 0;
    for (u32 i = 1; i < 4; ++i)
        sum += state->sz[i];
    s64 value = (s64)state->zsf[four ? 1 : 0] * sum;
    hm_gte_mac0(state, value);
    s64 depth = hm_gte_shift(value, 12);
    if (depth < 0 || depth > 65535)
        state->flags |= UINT32_C(1) << 18;
    state->otz = (u16)(depth < 0 ? 0 : depth > 65535 ? 65535 : depth);
}

static inline void hm_gte_command(hm_gte_state *state, u32 word) {
    u32 shift = (word & (1u << 19)) ? 12 : 0;
    int lm = (word & (1u << 10)) != 0;
    state->flags = 0;
    switch (word & 63) {
    case 0x06:
        hm_gte_clip(state);
        break;
    case 0x0C:
        hm_gte_cross(state, shift, lm);
        break;
    case 0x12:
        hm_gte_multiply(state, (word >> 17) & 3, (word >> 15) & 3, (word >> 13) & 3, shift, lm);
        break;
    case 0x28:
        hm_gte_square(state, shift, lm);
        break;
    case 0x2D:
        hm_gte_average(state, 0);
        break;
    case 0x2E:
        hm_gte_average(state, 1);
        break;
    default:
        hm_runtime_error("hm_gte_command", "GTE command is not implemented");
    }
    hm_gte_summary(state);
}

#endif

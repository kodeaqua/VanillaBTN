#include "psx/spu_decode.h"

static s32 hm_spu_floor(s32 value, u32 divisor) {
    return value >= 0 ? value / (s32)divisor : -1 - ((-1 - value) / (s32)divisor);
}
static s32 hm_spu_saturate(s32 value) {
    return value < -32768 ? -32768 : value > 32767 ? 32767 : value;
}

int hm_spu_decode_block(const u8 block[16], hm_adpcm_history *history, s16 output[28]) {
    static const s16 predictor[5][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}, {122, -60}};
    u32 filter = block[0] >> 4, shift = block[0] & 15;
    if (filter > 4)
        return 0;
    if (shift > 12)
        shift = 9;
    for (u32 i = 0; i < 28; ++i) {
        s32 nibble = (block[2 + i / 2] >> ((i & 1) * 4)) & 15;
        if (nibble >= 8)
            nibble -= 16;
        s32 residual = hm_spu_floor(nibble * 4096, 1u << shift);
        s32 predicted = hm_spu_floor(history->previous * predictor[filter][0] +
                                         history->older * predictor[filter][1] + 32,
                                     64);
        s32 sample = hm_spu_saturate(residual + predicted);
        history->older = history->previous;
        history->previous = sample;
        output[i] = (s16)sample;
    }
    return 1;
}

void hm_spu_adsr_key_on(hm_adsr *envelope, u16 adsr1, u16 adsr2) {
    *envelope = (hm_adsr){adsr1, adsr2, 0, 0, HM_ADSR_ATTACK};
}
void hm_spu_adsr_key_off(hm_adsr *envelope) {
    if (envelope->phase != HM_ADSR_OFF) {
        envelope->phase = HM_ADSR_RELEASE;
        envelope->counter = 0;
    }
}
s32 hm_spu_adsr_tick(hm_adsr *envelope) {
    u32 shift, step_bits = 0, all_bits = 31;
    int exponential = 0, decreasing = 0;
    switch (envelope->phase) {
    case HM_ADSR_ATTACK:
        shift = (envelope->adsr1 >> 10) & 31;
        step_bits = (envelope->adsr1 >> 8) & 3;
        exponential = (envelope->adsr1 >> 15) & 1;
        all_bits = 127;
        break;
    case HM_ADSR_DECAY:
        shift = (envelope->adsr1 >> 4) & 15;
        exponential = decreasing = 1;
        all_bits = 0xFFFFFFFFu; /* Decay's four-bit shift cannot freeze. */
        break;
    case HM_ADSR_SUSTAIN:
        shift = (envelope->adsr2 >> 8) & 31;
        step_bits = (envelope->adsr2 >> 6) & 3;
        exponential = (envelope->adsr2 >> 15) & 1;
        decreasing = (envelope->adsr2 >> 14) & 1;
        all_bits = 127;
        break;
    case HM_ADSR_RELEASE:
        shift = envelope->adsr2 & 31;
        decreasing = 1;
        exponential = (envelope->adsr2 >> 5) & 1;
        break;
    default:
        return 0;
    }
    s32 step = decreasing ? -8 + (s32)step_bits : 7 - (s32)step_bits;
    if (shift < 11)
        step *= (s32)(1u << (11 - shift));
    u32 increment = 0x8000u >> (shift > 11 ? shift - 11 : 0);
    if (exponential && !decreasing && envelope->level > 0x6000) {
        if (shift < 10)
            step /= 4;
        else if (shift >= 11)
            increment /= 4;
        else {
            step /= 2;
            increment /= 2;
        }
    } else if (exponential && decreasing) {
        step = hm_spu_floor(step * envelope->level, 0x8000);
    }
    /* Release has no step bits; its all-ones rate is just the shift. */
    u32 rate = envelope->phase == HM_ADSR_RELEASE ? shift : (shift << 2) | step_bits;
    if (rate != all_bits && !increment)
        increment = 1;
    envelope->counter += increment;
    if (!(envelope->counter & 0x8000))
        return envelope->level;
    envelope->counter &= 0x7FFF;
    envelope->level += step;
    if (envelope->level < 0)
        envelope->level = 0;
    if (envelope->level > 32767)
        envelope->level = 32767;
    u32 next = envelope->phase;
    if (next == HM_ADSR_ATTACK && envelope->level == 32767)
        next = HM_ADSR_DECAY;
    else if (next == HM_ADSR_DECAY && envelope->level <= ((envelope->adsr1 & 15) + 1) * 2048)
        next = HM_ADSR_SUSTAIN;
    else if (next == HM_ADSR_RELEASE && !envelope->level)
        next = HM_ADSR_OFF;
    if (next != envelope->phase) {
        envelope->phase = (u8)next;
        envelope->counter = 0;
    }
    return envelope->level;
}

#include "ps1_reverb.h"
#include <string.h>

/* Hardware register values and the SPU's 39-tap half-band FIR:
 * https://psx-spx.consoledev.net/soundprocessingunitspu/
 * SDK mode order is off, room, studio A/B/C, hall, space, echo, delay, pipe. */
static const struct { unsigned bytes; uint16_t registers[32]; } presets[10] = {
    {0x10, {
        0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
    }},
    {0x26c0, {
        0x007d, 0x005b, 0x6d80, 0x54b8, 0xbed0, 0x0000, 0x0000, 0xba80,
        0x5800, 0x5300, 0x04d6, 0x0333, 0x03f0, 0x0227, 0x0374, 0x01ef,
        0x0334, 0x01b5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x01b4, 0x0136, 0x00b8, 0x005c, 0x8000, 0x8000,
    }},
    {0x1f40, {
        0x0033, 0x0025, 0x70f0, 0x4fa8, 0xbce0, 0x4410, 0xc0f0, 0x9c00,
        0x5280, 0x4ec0, 0x03e4, 0x031b, 0x03a4, 0x02af, 0x0372, 0x0266,
        0x031c, 0x025d, 0x025c, 0x018e, 0x022f, 0x0135, 0x01d2, 0x00b7,
        0x018f, 0x00b5, 0x00b4, 0x0080, 0x004c, 0x0026, 0x8000, 0x8000,
    }},
    {0x4840, {
        0x00b1, 0x007f, 0x70f0, 0x4fa8, 0xbce0, 0x4510, 0xbef0, 0xb4c0,
        0x5280, 0x4ec0, 0x0904, 0x076b, 0x0824, 0x065f, 0x07a2, 0x0616,
        0x076c, 0x05ed, 0x05ec, 0x042e, 0x050f, 0x0305, 0x0462, 0x02b7,
        0x042f, 0x0265, 0x0264, 0x01b2, 0x0100, 0x0080, 0x8000, 0x8000,
    }},
    {0x6fe0, {
        0x00e3, 0x00a9, 0x6f60, 0x4fa8, 0xbce0, 0x4510, 0xbef0, 0xa680,
        0x5680, 0x52c0, 0x0dfb, 0x0b58, 0x0d09, 0x0a3c, 0x0bd9, 0x0973,
        0x0b59, 0x08da, 0x08d9, 0x05e9, 0x07ec, 0x04b0, 0x06ef, 0x03d2,
        0x05ea, 0x031d, 0x031c, 0x0238, 0x0154, 0x00aa, 0x8000, 0x8000,
    }},
    {0xade0, {
        0x01a5, 0x0139, 0x6000, 0x5000, 0x4c00, 0xb800, 0xbc00, 0xc000,
        0x6000, 0x5c00, 0x15ba, 0x11bb, 0x14c2, 0x10bd, 0x11bc, 0x0dc1,
        0x11c0, 0x0dc3, 0x0dc0, 0x09c1, 0x0bc4, 0x07c1, 0x0a00, 0x06cd,
        0x09c2, 0x05c1, 0x05c0, 0x041a, 0x0274, 0x013a, 0x8000, 0x8000,
    }},
    {0xf6c0, {
        0x033d, 0x0231, 0x7e00, 0x5000, 0xb400, 0xb000, 0x4c00, 0xb000,
        0x6000, 0x5400, 0x1ed6, 0x1a31, 0x1d14, 0x183b, 0x1bc2, 0x16b2,
        0x1a32, 0x15ef, 0x15ee, 0x1055, 0x1334, 0x0f2d, 0x11f6, 0x0c5d,
        0x1056, 0x0ae1, 0x0ae0, 0x07a2, 0x0464, 0x0232, 0x8000, 0x8000,
    }},
    {0x18040, {
        0x0001, 0x0001, 0x7fff, 0x7fff, 0x0000, 0x0000, 0x0000, 0x8100,
        0x0000, 0x0000, 0x1fff, 0x0fff, 0x1005, 0x0005, 0x0000, 0x0000,
        0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x1004, 0x1002, 0x0004, 0x0002, 0x8000, 0x8000,
    }},
    {0x18040, {
        0x0001, 0x0001, 0x7fff, 0x7fff, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x1fff, 0x0fff, 0x1005, 0x0005, 0x0000, 0x0000,
        0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x1004, 0x1002, 0x0004, 0x0002, 0x8000, 0x8000,
    }},
    {0x3c00, {
        0x0017, 0x0013, 0x70f0, 0x4fa8, 0xbce0, 0x4510, 0xbef0, 0x8500,
        0x5f80, 0x54c0, 0x0371, 0x02af, 0x02e5, 0x01df, 0x02b0, 0x01d7,
        0x0358, 0x026a, 0x01d6, 0x011e, 0x012d, 0x00b1, 0x011f, 0x0059,
        0x01a0, 0x00e3, 0x0058, 0x0040, 0x0028, 0x0014, 0x8000, 0x8000,
    }},
};
static const int16_t half_band[39] = {
    -1,0,2,0,-10,0,35,0,-103,0,266,0,-616,0,1332,0,-2960,0,
    10246,16384,10246,0,-2960,0,1332,0,-616,0,266,0,-103,0,35,0,-10,0,2,0,-1
};

static int16_t saturate(int64_t value) {
    if (value > 32767) return 32767;
    if (value < -32768) return -32768;
    return (int16_t)value;
}
static int32_t multiply(int32_t value, int16_t gain) {
    return (int32_t)(((int64_t)value * gain) >> 15);
}
/* Register addresses are in eight-byte units; DSP offsets are halfwords.
 * Wrapping is relative to ESA, including negative all-pass displacements. */
static unsigned address(const Ps1Reverb *r, int offset) {
    int length = PS1_REVERB_WORDS - (int)r->base;
    int relative = ((int)r->cursor - (int)r->base + offset) % length;
    if (relative < 0) relative += length;
    return r->base + relative;
}
static int16_t read_ram(const Ps1Reverb *r, int offset) {
    return r->ram[address(r, offset)];
}
static void write_ram(Ps1Reverb *r, int offset, int64_t value) {
    r->ram[address(r, offset)] = saturate(value);
}

int ps1_reverb_set_registers(Ps1Reverb *r, const uint16_t registers[32],
                            unsigned bytes, int clear) {
    if (!r || !registers || bytes < 8 || bytes > PS1_REVERB_WORDS * 2 || bytes % 8) return 0;
    r->base = PS1_REVERB_WORDS - bytes / 2;
    r->cursor = r->base;
    memcpy(r->registers, registers, sizeof(r->registers));
    if (clear) memset(r->ram + r->base, 0, bytes);
    return 1;
}
int ps1_reverb_set_preset(Ps1Reverb *r, int type, int clear) {
    if (type < 0 || type >= 10) return 0;
    return ps1_reverb_set_registers(r, presets[type].registers, presets[type].bytes, clear);
}
void ps1_reverb_set_echo(Ps1Reverb *r, int type, int delay, int feedback) {
    if (type != 7 && type != 8) return;
    if (delay < 0) delay = 0;
    if (delay > 127) delay = 127;
    if (feedback < 0) feedback = 0;
    if (feedback > 127) feedback = 127;
    uint16_t *v = r->registers;
    unsigned full = (unsigned)delay * 8192 / 127;
    unsigned half = (unsigned)delay * 4096 / 127;
    v[RV_MLSAME] = (uint16_t)(full - v[RV_DAPF1]);
    v[RV_MRSAME] = (uint16_t)(half - v[RV_DAPF2]);
    v[RV_DLSAME] = (uint16_t)(v[RV_DRSAME] + half);
    v[RV_MLCOMB1] = (uint16_t)(v[RV_MRCOMB1] + half);
    v[RV_MLAPF1] = (uint16_t)(v[RV_MLAPF2] + half);
    v[RV_MRAPF1] = (uint16_t)(v[RV_MRAPF2] + half);
    v[RV_VWALL] = (uint16_t)(feedback * 33024 / 127);
}

int16_t ps1_reverb_step(Ps1Reverb *r, unsigned c, int16_t input, int write_enable) {
    const uint16_t *v = r->registers;
    /* IIR reflections read the previous halfword, not the previous frame. */
    if (write_enable) {
        int32_t source = multiply(input, (int16_t)v[RV_VLIN + c]);
        unsigned destinations[2] = {RV_MLSAME + c, RV_MLDIFF + c};
        unsigned reflections[2] = {RV_DLSAME + c, RV_DLDIFF + (c ^ 1)};
        for (unsigned side = 0; side < 2; side++) {
            int at = v[destinations[side]] * 4;
            int previous = read_ram(r, at - 1);
            int reflected = multiply(read_ram(r, v[reflections[side]] * 4), (int16_t)v[RV_VWALL]);
            int value = multiply(source + reflected - previous, (int16_t)v[RV_VIIR]) + previous;
            /* Documented SPU edge case: IIR -1 negates the entire result. */
            if (v[RV_VIIR] == 0x8000) value = -value;
            write_ram(r, at, value);
        }
    }
    /* Reads/output continue with master writes disabled, including APFs. */
    unsigned comb[4] = {RV_MLCOMB1 + c, RV_MLCOMB2 + c, RV_MLCOMB3 + c, RV_MLCOMB4 + c};
    int32_t sum = 0;
    for (unsigned tap = 0; tap < 4; tap++) {
        sum += multiply(read_ram(r, v[comb[tap]] * 4), (int16_t)v[RV_VCOMB1 + tap]);
    }
    int32_t output = saturate(sum);
    for (unsigned apf = 0; apf < 2; apf++) {
        int at = v[RV_MLAPF1 + apf * 2 + c] * 4;
        int delayed = read_ram(r, at - v[RV_DAPF1 + apf] * 4);
        int gain = (int16_t)v[RV_VAPF1 + apf];
        int value = saturate(output - multiply(delayed, (int16_t)gain));
        if (write_enable) write_ram(r, at, value);
        output = saturate(multiply(value, (int16_t)gain) + delayed);
    }
    if (c) r->cursor = address(r, 1);
    return (int16_t)output;
}

void ps1_reverb_clock(Ps1Reverb *r, int32_t left, int32_t right,
                       int write_enable, int16_t output[2]) {
    unsigned p = r->history_position;
    unsigned c = r->phase; /* left/right alternate on successive clocks */
    r->input_history[p][0] = saturate(left);
    r->input_history[p][1] = saturate(right);
    int64_t down = 0;
    for (unsigned tap = 0; tap < 39; tap++) {
        down += (int32_t)r->input_history[(p - tap) & 63][c] * half_band[tap];
    }
    r->output_history[p][0] = r->output_history[p][1] = 0;
    r->output_history[p][c] = ps1_reverb_step(r, c, saturate(down >> 15), write_enable);
    for (unsigned channel = 0; channel < 2; channel++) {
        int64_t up = 0;
        for (unsigned tap = 0; tap < 39; tap++) {
            up += (int32_t)r->output_history[(p - tap) & 63][channel] * half_band[tap];
        }
        output[channel] = saturate(up >> 14); /* compensate zero-stuffing */
    }
    r->history_position = (p + 1) & 63;
    r->phase ^= 1;
}

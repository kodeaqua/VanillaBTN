#include "psx/reverb.h"
#include "psx/error.h"
#include "psx/sound_control.h"
#include <libsnd.h>
#include <string.h>

/* Numeric hardware presets from psx-spx's SPU register specification. */
static const u16 hm_presets[3][32] = {
    {0x007D, 0x005B, 0x6D80, 0x54B8, 0xBED0, 0,      0,      0xBA80, 0x5800, 0x5300, 0x04D6,
     0x0333, 0x03F0, 0x0227, 0x0374, 0x01EF, 0x0334, 0x01B5, 0,      0,      0,      0,
     0,      0,      0,      0,      0x01B4, 0x0136, 0x00B8, 0x005C, 0x8000, 0x8000},
    {0x00E3, 0x00A9, 0x6F60, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0xA680, 0x5680, 0x52C0, 0x0DFB,
     0x0B58, 0x0D09, 0x0A3C, 0x0BD9, 0x0973, 0x0B59, 0x08DA, 0x08D9, 0x05E9, 0x07EC, 0x04B0,
     0x06EF, 0x03D2, 0x05EA, 0x031D, 0x031C, 0x0238, 0x0154, 0x00AA, 0x8000, 0x8000},
    {0x01A5, 0x0139, 0x6000, 0x5000, 0x4C00, 0xB800, 0xBC00, 0xC000, 0x6000, 0x5C00, 0x15BA,
     0x11BB, 0x14C2, 0x10BD, 0x11BC, 0x0DC1, 0x11C0, 0x0DC3, 0x0DC0, 0x09C1, 0x0BC4, 0x07C1,
     0x0A00, 0x06CD, 0x09C2, 0x05C1, 0x05C0, 0x041A, 0x0274, 0x013A, 0x8000, 0x8000}};
static const s16 hm_fir[39] = {-1,    0, 2,     0, -10,  0, 35,    0, -103,  0,
                               266,   0, -616,  0, 1332, 0, -2960, 0, 10246, 16384,
                               10246, 0, -2960, 0, 1332, 0, -616,  0, 266,   0,
                               -103,  0, 35,    0, -10,  0, 2,     0, -1};
static struct {
    s16 memory[0xADE0 / 2], input[2][64], output[2][64];
    u16 registers[32];
    u32 size, cursor, fir_cursor, parity;
    s32 depth[2];
} hm_reverb;

static s32 hm_rev_floor(s64 value) {
    return (s32)(value >= 0 ? value / 32768 : -1 - ((-1 - value) / 32768));
}
static s16 hm_rev_clamp(s32 value) {
    return (s16)(value < -32768 ? -32768 : value > 32767 ? 32767 : value);
}
static s32 hm_rev_signed(u16 value) { return value < 32768 ? value : (s32)value - 65536; }
static s32 hm_rev_multiply(s32 value, u32 coefficient) {
    return hm_rev_floor((s64)value * hm_rev_signed(hm_reverb.registers[coefficient]));
}
static u32 hm_rev_address(s32 relative) {
    s32 address = ((s32)hm_reverb.cursor + relative) % (s32)hm_reverb.size;
    return (u32)(address < 0 ? address + (s32)hm_reverb.size : address);
}
static s32 hm_rev_read(s32 relative) { return hm_reverb.memory[hm_rev_address(relative)]; }
static void hm_rev_write(s32 relative, s32 value) {
    if (hm_sound_configuration()->reverb)
        hm_reverb.memory[hm_rev_address(relative)] = hm_rev_clamp(value);
}
static s32 hm_rev_offset(u32 index) { return (s32)hm_reverb.registers[index] * 4; }
void hm_reverb_reset(void) { memset(&hm_reverb, 0, sizeof(hm_reverb)); }
s32 SsUtSetReverbType(s32 type) {
    s32 preset = type == 1 ? 0 : type == 4 ? 1 : type == 5 ? 2 : -1;
    if (type && preset < 0)
        hm_runtime_error("SsUtSetReverbType", "unsupported reverb preset");
    hm_reverb_reset();
    if (type) {
        memcpy(hm_reverb.registers, hm_presets[preset], sizeof(hm_reverb.registers));
        hm_reverb.size = (type == 1 ? 0x26C0u : type == 4 ? 0x6FE0u : 0xADE0u) / 2;
    }
    return type;
}
void SsUtSetReverbDepth(s32 left, s32 right) {
    if (left < 0 || left > 127 || right < 0 || right > 127)
        hm_runtime_error("SsUtSetReverbDepth", "unsupported depth");
    hm_reverb.depth[0] = left * 32767 / 127;
    hm_reverb.depth[1] = right * 32767 / 127;
}
void SsSetRVol(s32 left, s32 right) { SsUtSetReverbDepth(left, right); }

static void hm_rev_network(const s32 input[2], s32 output[2]) {
    for (u32 side = 0; side < 2; ++side) {
        s32 incoming = hm_rev_multiply(input[side], 30 + side);
        s32 same = hm_rev_offset(10 + side), diff = hm_rev_offset(18 + side);
        s32 previous = hm_rev_read(same - 1);
        hm_rev_write(
            same,
            previous +
                hm_rev_multiply(
                    incoming + hm_rev_multiply(hm_rev_read(hm_rev_offset(16 + side)), 7) - previous,
                    2));
        previous = hm_rev_read(diff - 1);
        hm_rev_write(diff, previous + hm_rev_multiply(
                                          incoming +
                                              hm_rev_multiply(
                                                  hm_rev_read(hm_rev_offset(24 + (side ^ 1))), 7) -
                                              previous,
                                          2));
        s32 value = hm_rev_multiply(hm_rev_read(hm_rev_offset(12 + side)), 3) +
                    hm_rev_multiply(hm_rev_read(hm_rev_offset(14 + side)), 4) +
                    hm_rev_multiply(hm_rev_read(hm_rev_offset(20 + side)), 5) +
                    hm_rev_multiply(hm_rev_read(hm_rev_offset(22 + side)), 6);
        for (u32 apf = 0; apf < 2; ++apf) {
            s32 address = hm_rev_offset(26 + apf * 2 + side);
            s32 delayed = hm_rev_read(address - hm_rev_offset(apf));
            value -= hm_rev_multiply(delayed, 8 + apf);
            hm_rev_write(address, value);
            value = hm_rev_multiply(value, 8 + apf) + delayed;
        }
        output[side] = hm_rev_clamp(value);
    }
    hm_reverb.cursor = (hm_reverb.cursor + 1) % hm_reverb.size;
}

static s32 hm_rev_filter(const s16 *history) {
    s64 value = 0;
    for (u32 tap = 0; tap < 39; ++tap)
        value += (s64)history[(hm_reverb.fir_cursor - tap) & 63] * hm_fir[tap];
    return hm_rev_floor(value);
}
void hm_reverb_sample(s32 left, s32 right, s32 output[2]) {
    output[0] = output[1] = 0;
    if (!hm_reverb.size)
        return;
    u32 cursor = hm_reverb.fir_cursor;
    hm_reverb.input[0][cursor] = hm_rev_clamp(left);
    hm_reverb.input[1][cursor] = hm_rev_clamp(right);
    s32 wet[2] = {0};
    if (!hm_reverb.parity) {
        s32 input[2] = {hm_rev_filter(hm_reverb.input[0]), hm_rev_filter(hm_reverb.input[1])};
        hm_rev_network(input, wet);
    }
    for (u32 side = 0; side < 2; ++side) {
        hm_reverb.output[side][cursor] = hm_rev_clamp(wet[side]);
        output[side] =
            hm_rev_floor((s64)hm_rev_filter(hm_reverb.output[side]) * 2 * hm_reverb.depth[side]);
    }
    hm_reverb.parity ^= 1;
    hm_reverb.fir_cursor = (cursor + 1) & 63;
}

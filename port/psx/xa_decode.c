#include "psx/xa_decode.h"
#include "psx/gte_core.h"
#include <string.h>

static const s16 hm_xa_coefficients[29][7] = {
#include "psx/xa_zigzag.inc"
};
void hm_xa_decoder_reset(hm_xa_decoder *decoder) { memset(decoder, 0, sizeof(*decoder)); }
static s16 hm_xa_interpolate(const hm_xa_decoder *decoder, u32 channel, u32 phase) {
    s32 sum = 0;
    for (u32 tap = 0; tap < 29; ++tap)
        sum += (s32)hm_gte_shift((s64)decoder->ring[channel][(decoder->position - tap - 1) & 31] *
                                     hm_xa_coefficients[tap][phase],
                                 15);
    return (s16)(sum < -32768 ? -32768 : sum > 32767 ? 32767 : sum);
}
int hm_xa_decode_sector(hm_xa_decoder *decoder, const u8 raw[2352], s16 *stereo) {
    if (!decoder || !raw || !stereo || raw[15] != 2 || (raw[18] & 0x64) != 0x64 || raw[19] != 1 ||
        memcmp(raw + 16, raw + 20, 4))
        return 0;
    u32 frames = 0;
    for (u32 group = 0; group < 18; ++group) {
        const u8 *data = raw + 24 + group * 128;
        for (u32 block = 0; block < 4; ++block) {
            s16 decoded[2][28];
            for (u32 channel = 0; channel < 2; ++channel) {
                u8 packed[16] = {0};
                packed[0] = data[4 + block * 2 + channel] & 0x3F;
                for (u32 i = 0; i < 28; ++i) {
                    u32 nibble = (data[16 + i * 4 + block] >> (channel * 4)) & 15;
                    packed[2 + i / 2] |= (u8)(nibble << ((i & 1) * 4));
                }
                if (!hm_spu_decode_block(packed, &decoder->history[channel], decoded[channel]))
                    return 0;
            }
            for (u32 i = 0; i < 28; ++i) {
                decoder->ring[0][decoder->position & 31] = decoded[0][i];
                decoder->ring[1][decoder->position & 31] = decoded[1][i];
                decoder->position = (decoder->position + 1) & 31;
                if (++decoder->samples == 6) {
                    decoder->samples = 0;
                    for (u32 phase = 0; phase < 7; ++phase) {
                        stereo[frames * 2] = hm_xa_interpolate(decoder, 0, phase);
                        stereo[frames * 2 + 1] = hm_xa_interpolate(decoder, 1, phase);
                        ++frames;
                    }
                }
            }
        }
    }
    return frames == HM_XA_FRAMES;
}

#include "psx/sound_control.h"
#include "psx/sequence.h"
#include "psx/spu_voice.h"
#include "psx/error.h"
#include "psx/reverb.h"
#include <string.h>

static const s16 hm_gaussian_table[512] = {
#include "psx/spu_gauss.inc"
};
static s32 hm_voice_floor(s32 value) {
    return value >= 0 ? value / 32768 : -1 - ((-1 - value) / 32768);
}
static s16 hm_voice_clamp(s32 value) {
    return (s16)(value < -32768 ? -32768 : value > 32767 ? 32767 : value);
}
s16 hm_spu_gaussian(const s16 recent[4], u32 fraction) {
    u32 i = (fraction >> 4) & 255;
    s32 out = hm_voice_floor(hm_gaussian_table[255 - i] * recent[3]);
    out += hm_voice_floor(hm_gaussian_table[511 - i] * recent[2]);
    out += hm_voice_floor(hm_gaussian_table[256 + i] * recent[1]);
    out += hm_voice_floor(hm_gaussian_table[i] * recent[0]);
    return hm_voice_clamp(out);
}

static int hm_spu_hq_interpolation = 0;
void hm_spu_set_hq(int enabled) {
    hm_spu_hq_interpolation = enabled != 0;
}
int hm_spu_get_hq(void) {
    return hm_spu_hq_interpolation;
}

static s16 hm_spu_cubic_hermite(const s16 recent[4], u32 fraction) {
    float t = (float)(fraction & 4095) / 4096.0f;
    float p0 = (float)recent[3];
    float p1 = (float)recent[2];
    float p2 = (float)recent[1];
    float p3 = (float)recent[0];

    float c0 = p1;
    float c1 = 0.5f * (p2 - p0);
    float c2 = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
    float c3 = 0.5f * (p3 - p0) + 1.5f * (p1 - p2);

    float val = ((c3 * t + c2) * t + c1) * t + c0;
    return hm_voice_clamp((s32)val);
}

static void hm_voice_block(hm_spu_voice *voice) {
    if (voice->block > voice->bytes || voice->bytes - voice->block < 16)
        hm_runtime_error("SPU voice", "sample block exceeds bank asset");
    const u8 *block = voice->data + voice->block;
    voice->flags = block[1];
    if (voice->flags & 4)
        voice->loop = voice->block;
    if (!hm_spu_decode_block(block, &voice->history, voice->decoded))
        hm_runtime_error("SPU voice", "unsupported ADPCM predictor");
    voice->decoded_at = 0;
}

static void hm_voice_advance(hm_spu_voice *voice) {
    if (voice->decoded_at == 28) {
        if ((voice->flags & 3) == 1) {
            voice->envelope.phase = HM_ADSR_OFF;
            voice->envelope.level = 0;
            return;
        }
        voice->block = voice->flags & 1 ? voice->loop : voice->block + 16;
        hm_voice_block(voice);
    }
    memmove(voice->recent + 1, voice->recent, 3 * sizeof(s16));
    voice->recent[0] = voice->decoded[voice->decoded_at++];
}

void hm_spu_voice_start(hm_spu_voice *voice, const void *data, u32 bytes, u32 pitch, u16 adsr1,
                        u16 adsr2, s16 left, s16 right) {
    if (!voice || !data || bytes < 16 || (bytes & 15))
        hm_runtime_error("SPU voice", "invalid sample asset");
    memset(voice, 0, sizeof(*voice));
    voice->data = data;
    voice->bytes = bytes;
    voice->pitch = pitch > 0x3FFF ? 0x4000 : pitch;
    voice->left = left;
    voice->right = right;
    hm_spu_adsr_key_on(&voice->envelope, adsr1, adsr2);
    hm_voice_block(voice);
    hm_voice_advance(voice);
}

static void hm_voice_mix(hm_spu_voice voices[24], s16 *stereo, u32 frames, int reverb) {
    int bgm_vol = hm_sound_get_bgm_volume();
    int sfx_vol = hm_sound_get_sfx_volume();
    for (u32 frame = 0; frame < frames; ++frame) {
        s32 left = 0, right = 0, send_left = 0, send_right = 0;
        for (u32 i = 0; i < 24; ++i) {
            hm_spu_voice *voice = voices + i;
            if (voice->envelope.phase == HM_ADSR_OFF)
                continue;
            int vol_mult = hm_sound_sequence_voice_active(i) ? bgm_vol : sfx_vol;
            if (vol_mult <= 0) {
                hm_spu_adsr_tick(&voice->envelope);
                voice->fraction += voice->pitch;
                while (voice->fraction >= 4096 && voice->envelope.phase != HM_ADSR_OFF) {
                    voice->fraction -= 4096;
                    hm_voice_advance(voice);
                }
                continue;
            }
            s16 interp = hm_spu_hq_interpolation ? hm_spu_cubic_hermite(voice->recent, voice->fraction)
                                                  : hm_spu_gaussian(voice->recent, voice->fraction);
            s32 sample = hm_voice_floor(interp * hm_spu_adsr_tick(&voice->envelope));
            s32 l = hm_voice_floor(sample * voice->left), r = hm_voice_floor(sample * voice->right);
            if (vol_mult < 100) {
                l = (l * vol_mult) / 100;
                r = (r * vol_mult) / 100;
            }
            left += l;
            right += r;
            if (voice->reverb) {
                send_left += l;
                send_right += r;
            }
            voice->fraction += voice->pitch;
            while (voice->fraction >= 4096 && voice->envelope.phase != HM_ADSR_OFF) {
                voice->fraction -= 4096;
                hm_voice_advance(voice);
            }
        }
        if (reverb) {
            s32 wet[2];
            hm_reverb_sample(send_left, send_right, wet);
            left += wet[0];
            right += wet[1];
        }
        stereo[frame * 2] = hm_voice_clamp(left);
        stereo[frame * 2 + 1] = hm_voice_clamp(right);
    }
}
void hm_spu_voice_mix(hm_spu_voice voices[24], s16 *stereo, u32 frames) {
    hm_voice_mix(voices, stereo, frames, 0);
}
void hm_spu_voice_mix_reverb(hm_spu_voice voices[24], s16 *stereo, u32 frames) {
    hm_voice_mix(voices, stereo, frames, 1);
}

/* ps1_spu.c — host-side PS1 SPU compatibility layer. */

#include "ps1_spu.h"
#include <math.h>
#include <string.h>

#define VAB_MAGIC 0x56414270u /* "pBAV" in little-endian */
#define ADPCM_BLOCK_SIZE 16
#define ADPCM_SAMPLES_PER_BLOCK 28

/* PS1 SPU ADPCM filter coefficients (* 32) */
static const int K0[5] = { 0, 60, 115, 98, 122 };
static const int K1[5] = { 0,  0, -52, -55, -60 };

/* -------------------------------------------------------------------------
 * Low-level ADPCM block decode
 * ---------------------------------------------------------------------- */
uint8_t ps1_adpcm_decode_block(const uint8_t *block, int16_t *out,
                                int16_t *hist1, int16_t *hist2) {
    uint8_t hdr = block[0];
    uint8_t flags = block[1];
    int shift = hdr & 0x0F;
    int filter = (hdr >> 4) & 0x07;
    if (filter > 4) filter = 4;

    int f0 = K0[filter];
    int f1 = K1[filter];
    int s1 = *hist1;
    int s2 = *hist2;

    for (int i = 0; i < ADPCM_SAMPLES_PER_BLOCK; i++) {
        uint8_t byte = block[2 + i / 2];
        int nibble = (i & 1) ? (byte >> 4) : (byte & 0x0F);
        int n = (nibble & 0x08) ? (nibble | ~0x0F) : nibble;
        int sample = (n << shift) + ((f0 * s1 + f1 * s2 + 32) / 64);
        if (sample > 32767)  sample = 32767;
        if (sample < -32768) sample = -32768;
        out[i] = (int16_t)sample;
        s2 = s1;
        s1 = sample;
    }

    *hist1 = (int16_t)s1;
    *hist2 = (int16_t)s2;
    return flags;
}

/* -------------------------------------------------------------------------
 * Voice sample fetch helper
 * ---------------------------------------------------------------------- */
static int16_t fetch_voice_raw_sample(Ps1Voice *voice) {
    if (!voice->active) return 0;

    if (voice->block_sample_idx >= ADPCM_SAMPLES_PER_BLOCK) {
        if (voice->byte_offset + ADPCM_BLOCK_SIZE > voice->sample_len) {
            voice->active = 0;
            return 0;
        }

        const uint8_t *block = voice->sample_data + voice->byte_offset;
        uint8_t flags = ps1_adpcm_decode_block(block, voice->block,
                                                &voice->hist1, &voice->hist2);

        if (flags & PS1_ADPCM_FLAG_LOOP_START) {
            voice->loop_offset = voice->byte_offset;
            voice->has_loop = 1;
        }

        voice->byte_offset += ADPCM_BLOCK_SIZE;
        voice->block_sample_idx = 0;

        if (flags & PS1_ADPCM_FLAG_LOOP_END) {
            if ((flags & PS1_ADPCM_FLAG_LOOP_REPEAT) && voice->has_loop) {
                voice->byte_offset = voice->loop_offset;
            } else {
                voice->active = 0;
            }
        }
    }

    return voice->block[voice->block_sample_idx++];
}

/* -------------------------------------------------------------------------
 * SPU Lifecycle
 * ---------------------------------------------------------------------- */
void ps1_spu_init(Ps1Spu *spu) {
    memset(spu, 0, sizeof(*spu));
    spu->master_vol_left = 0x3FFF;
    spu->master_vol_right = 0x3FFF;
    spu->cd_vol_left = 0x1FFF;
    spu->cd_vol_right = 0x1FFF;
}

Ps1VabId ps1_spu_load_vab(Ps1Spu *spu, const uint8_t *header, uint32_t header_size,
                          const uint8_t *body, uint32_t body_size) {
    if (!header || header_size < 32) return PS1_VAB_NONE;

    uint32_t magic;
    memcpy(&magic, header, 4);
    if (magic != VAB_MAGIC) return PS1_VAB_NONE;

    int slot = PS1_VAB_NONE;
    for (int i = 0; i < PS1_SPU_VAB_SLOTS; i++) {
        if (!spu->vabs[i].loaded) {
            slot = i;
            break;
        }
    }
    if (slot == PS1_VAB_NONE) return PS1_VAB_NONE;

    Ps1VabSlot *v = &spu->vabs[slot];
    memset(v, 0, sizeof(*v));
    v->loaded = 1;
    v->header = header;
    v->header_size = header_size;

    if (body) {
        v->body = body;
        v->body_size = body_size;
    } else {
        /* Combined VAB: body starts after header */
        v->body = header;
        v->body_size = header_size;
    }

    memcpy(&v->num_progs, header + 20, 2);
    memcpy(&v->num_tones, header + 22, 2);
    memcpy(&v->num_vags,  header + 24, 2);

    /* VAG offset table location:
     * Header layout:
     *   32 bytes (VabHdr)
     * + 2048 bytes (128 ProgAtr)
     * + (16 * num_progs * 32 bytes) (VagAtr)
     * Followed by uint16_t vag_offsets[256].
     */
    uint32_t tone_count = (uint32_t)v->num_progs * 16u;
    uint32_t offtbl_pos = 32u + 2048u + tone_count * 32u;
    if (offtbl_pos + 512u <= header_size) {
        const uint16_t *offtbl = (const uint16_t *)(const void *)(header + offtbl_pos);
        uint32_t cum_offset = 16u; /* first sample starts after 16 bytes dummy block */
        for (uint32_t i = 1; i <= v->num_vags && i < 256; i++) {
            v->vag_body_offsets[i] = cum_offset;
            cum_offset += (uint32_t)offtbl[i] * 8u;
        }
    }

    return slot;
}

void ps1_spu_free_vab(Ps1Spu *spu, Ps1VabId id) {
    if (id < 0 || id >= PS1_SPU_VAB_SLOTS) return;
    for (int v = 0; v < PS1_SPU_VOICES; v++) {
        if (spu->voices[v].vab_id == id) {
            memset(&spu->voices[v], 0, sizeof(spu->voices[v]));
        }
    }
    memset(&spu->vabs[id], 0, sizeof(spu->vabs[id]));
}

/* -------------------------------------------------------------------------
 * Voice Key On / Off
 * ---------------------------------------------------------------------- */
void ps1_spu_key_on_raw(Ps1Spu *spu, int voice_idx, const uint8_t *data,
                        uint32_t data_len, uint32_t pitch_16_16,
                        int16_t vol_left, int16_t vol_right) {
    if (voice_idx < 0 || voice_idx >= PS1_SPU_VOICES || !data || data_len < ADPCM_BLOCK_SIZE) return;

    Ps1Voice *v = &spu->voices[voice_idx];
    memset(v, 0, sizeof(*v));
    v->active = 1;
    v->vab_id = PS1_VAB_NONE;
    v->sample_data = data;
    v->sample_len = data_len;
    v->block_sample_idx = ADPCM_SAMPLES_PER_BLOCK;
    v->pitch_step = pitch_16_16 ? pitch_16_16 : 0x10000;
    v->vol_left = vol_left;
    v->vol_right = vol_right;

    v->curr_sample = fetch_voice_raw_sample(v);
    v->next_sample = fetch_voice_raw_sample(v);
}

void ps1_spu_key_on_vab(Ps1Spu *spu, int voice_idx, Ps1VabId vab_id,
                        int prog, int tone, int note, int fine,
                        int voll, int volr) {
    if (voice_idx < 0 || voice_idx >= PS1_SPU_VOICES) return;
    if (vab_id < 0 || vab_id >= PS1_SPU_VAB_SLOTS || !spu->vabs[vab_id].loaded) return;

    Ps1VabSlot *slot = &spu->vabs[vab_id];
    if (prog >= (int)slot->num_progs || tone >= 16) return;

    /* VagAtr offset */
    uint32_t vag_atr_offset = 32u + 2048u + ((uint32_t)prog * 16u + (uint32_t)tone) * 32u;
    if (vag_atr_offset + 32u > slot->header_size) return;

    const uint8_t *atr = slot->header + vag_atr_offset;
    uint8_t tone_vol = atr[2];
    uint8_t center_note = atr[4];
    uint16_t vag_idx;
    memcpy(&vag_idx, atr + 22, 2);

    if (vag_idx == 0 || vag_idx >= 256) return;
    uint32_t body_off = slot->vag_body_offsets[vag_idx];
    if (body_off >= slot->body_size) return;

    /* Pitch calculation: semitone ratio relative to center note */
    double semitones = (double)(note - (int)center_note) + (double)fine / 100.0;
    double ratio = pow(2.0, semitones / 12.0);
    uint32_t pitch = (uint32_t)(ratio * 65536.0 + 0.5);
    if (pitch == 0) pitch = 0x10000;

    /* Volume scaling: scale 0..127 to 0..0x7FFF */
    int32_t vl = ((int32_t)voll * (int32_t)tone_vol * 0x7FFF) / (127 * 127);
    int32_t vr = ((int32_t)volr * (int32_t)tone_vol * 0x7FFF) / (127 * 127);
    if (vl > 0x7FFF) vl = 0x7FFF;
    if (vr > 0x7FFF) vr = 0x7FFF;

    Ps1Voice *v = &spu->voices[voice_idx];
    memset(v, 0, sizeof(*v));
    v->active = 1;
    v->vab_id = vab_id;
    v->sample_data = slot->body + body_off;
    v->sample_len = slot->body_size - body_off;
    v->block_sample_idx = ADPCM_SAMPLES_PER_BLOCK;
    v->pitch_step = pitch;
    v->vol_left = (int16_t)vl;
    v->vol_right = (int16_t)vr;

    v->curr_sample = fetch_voice_raw_sample(v);
    v->next_sample = fetch_voice_raw_sample(v);
}

void ps1_spu_key_off(Ps1Spu *spu, int voice_idx) {
    if (voice_idx < 0 || voice_idx >= PS1_SPU_VOICES) return;
    spu->voices[voice_idx].active = 0;
}

void ps1_spu_key_off_all(Ps1Spu *spu) {
    for (int v = 0; v < PS1_SPU_VOICES; v++) {
        spu->voices[v].active = 0;
    }
}

int ps1_spu_voice_is_active(const Ps1Spu *spu, int voice_idx) {
    if (voice_idx < 0 || voice_idx >= PS1_SPU_VOICES) return 0;
    return spu->voices[voice_idx].active;
}

void ps1_spu_set_master_vol(Ps1Spu *spu, int16_t left, int16_t right) {
    spu->master_vol_left = left;
    spu->master_vol_right = right;
}

void ps1_spu_set_cd_vol(Ps1Spu *spu, int16_t left, int16_t right) {
    spu->cd_vol_left = left;
    spu->cd_vol_right = right;
}

/* -------------------------------------------------------------------------
 * Mixer
 * ---------------------------------------------------------------------- */
void ps1_spu_mix(Ps1Spu *spu, int16_t *out, int frames, int accumulate) {
    if (!spu || !out || frames <= 0) return;

    int32_t mvol_l = spu->master_vol_left;
    int32_t mvol_r = spu->master_vol_right;

    for (int f = 0; f < frames; f++) {
        int32_t mix_l = 0;
        int32_t mix_r = 0;

        for (int v = 0; v < PS1_SPU_VOICES; v++) {
            Ps1Voice *voice = &spu->voices[v];
            if (!voice->active) continue;

            /* Linear interpolation */
            int32_t frac = (int32_t)(voice->frac_pos & 0xFFFF);
            int32_t diff = (int32_t)voice->next_sample - (int32_t)voice->curr_sample;
            int32_t samp = (int32_t)voice->curr_sample + ((diff * frac) >> 16);

            mix_l += (samp * (int32_t)voice->vol_left) >> 15;
            mix_r += (samp * (int32_t)voice->vol_right) >> 15;

            voice->frac_pos += voice->pitch_step;
            while (voice->frac_pos >= 0x10000 && voice->active) {
                voice->curr_sample = voice->next_sample;
                voice->next_sample = fetch_voice_raw_sample(voice);
                voice->frac_pos -= 0x10000;
            }
        }

        /* The sum of 24 voices can exceed 16-bit range before master scaling. */
        int32_t out_l = (int32_t)(((int64_t)mix_l * mvol_l) >> 15);
        int32_t out_r = (int32_t)(((int64_t)mix_r * mvol_r) >> 15);

        if (accumulate) {
            out_l += out[f * 2 + 0];
            out_r += out[f * 2 + 1];
        }

        if (out_l >  32767) out_l =  32767;
        if (out_l < -32768) out_l = -32768;
        if (out_r >  32767) out_r =  32767;
        if (out_r < -32768) out_r = -32768;

        out[f * 2 + 0] = (int16_t)out_l;
        out[f * 2 + 1] = (int16_t)out_r;
    }
}

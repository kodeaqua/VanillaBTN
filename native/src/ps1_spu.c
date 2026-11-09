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
    if (shift > 12) shift = 9;
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
        int sample = ((n * 4096) >> shift) + ((f0 * s1 + f1 * s2 + 32) >> 6);
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
        if (voice->stop_after_block || voice->byte_offset > voice->sample_len
            || voice->sample_len - voice->byte_offset < ADPCM_BLOCK_SIZE) {
            voice->source_ended = 1;
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
            if (flags & PS1_ADPCM_FLAG_LOOP_REPEAT) {
                voice->byte_offset = voice->loop_offset;
            } else {
                voice->stop_after_block = 1;
            }
        }
    }

    return voice->block[voice->block_sample_idx++];
}

/* -------------------------------------------------------------------------
 * ADSR Envelope Generator
 * ---------------------------------------------------------------------- */
static void voice_init_adsr(Ps1Voice *v, uint16_t adsr1, uint16_t adsr2) {
    v->adsr1 = adsr1;
    v->adsr2 = adsr2;
    if (adsr1 == 0 && adsr2 == 0) {
        /* Bypassed ADSR: full volume immediately, instant key-off */
        v->env_phase = PS1_ADSR_SUSTAIN;
        v->env_level = 0x7FFF;
        v->adsr_release_rate = 0;
        return;
    }
    v->adsr_attack_mode = (adsr1 >> 15) & 1;
    v->adsr_attack_rate = (adsr1 >> 8) & 0x7F;
    v->adsr_decay_rate = (adsr1 >> 4) & 0x0F;
    int sl_raw = adsr1 & 0x0F;
    v->adsr_sustain_level = ((sl_raw + 1) * 0x7FFF) / 16;
    v->adsr_sustain_mode = (adsr2 >> 15) & 1;
    v->adsr_sustain_dir = (adsr2 >> 14) & 1;
    v->adsr_sustain_rate = (adsr2 >> 6) & 0x7F;
    v->adsr_release_mode = (adsr2 >> 5) & 1;
    v->adsr_release_rate = adsr2 & 0x1F;

    v->env_phase = PS1_ADSR_ATTACK;
    v->env_level = 0;
}

static void voice_step_adsr(Ps1Voice *v) {
    if (!v->active) return;

    switch (v->env_phase) {
    case PS1_ADSR_ATTACK: {
        int Ar = v->adsr_attack_rate;
        if (Ar >= 124) {
            v->env_level = 0x7FFF;
            v->env_phase = PS1_ADSR_DECAY;
        } else {
            int32_t step = (1 << (Ar / 8)) + ((Ar & 7) * (1 << (Ar / 8))) / 8;
            if (step < 1) step = 1;
            if (v->adsr_attack_mode == 0) {
                v->env_level += step;
            } else {
                v->env_level += step + ((0x7FFF - v->env_level) >> 8);
            }
            if (v->env_level >= 0x7FFF) {
                v->env_level = 0x7FFF;
                v->env_phase = PS1_ADSR_DECAY;
            }
        }
        break;
    }
    case PS1_ADSR_DECAY: {
        int Dr = v->adsr_decay_rate;
        int32_t diff = v->env_level - v->adsr_sustain_level;
        if (diff <= 0) {
            v->env_level = v->adsr_sustain_level;
            v->env_phase = PS1_ADSR_SUSTAIN;
        } else {
            int32_t dstep = (diff * (Dr + 1)) / 128 + 1;
            v->env_level -= dstep;
            if (v->env_level <= v->adsr_sustain_level) {
                v->env_level = v->adsr_sustain_level;
                v->env_phase = PS1_ADSR_SUSTAIN;
            }
        }
        break;
    }
    case PS1_ADSR_SUSTAIN: {
        if (v->adsr_sustain_dir == 0) {
            int32_t sstep = (1 << (v->adsr_sustain_rate / 8));
            v->env_level += sstep;
            if (v->env_level > 0x7FFF) v->env_level = 0x7FFF;
        } else {
            if (v->adsr_sustain_rate > 0) {
                int32_t sstep = (v->env_level * (v->adsr_sustain_rate + 1)) / 1024 + 1;
                v->env_level -= sstep;
                if (v->env_level <= 0) {
                    v->env_level = 0;
                    v->active = 0;
                    v->env_phase = PS1_ADSR_OFF;
                }
            }
        }
        break;
    }
    case PS1_ADSR_RELEASE: {
        if (v->adsr_release_rate == 0) {
            v->env_level = 0;
            v->active = 0;
            v->env_phase = PS1_ADSR_OFF;
        } else {
            int Rr = v->adsr_release_rate;
            int32_t rstep;
            if (v->adsr_release_mode == 0) {
                rstep = (1 << (Rr / 4)) + 1;
            } else {
                rstep = (v->env_level * (Rr + 1)) / 128 + 1;
            }
            v->env_level -= rstep;
            if (v->env_level <= 0) {
                v->env_level = 0;
                v->active = 0;
                v->env_phase = PS1_ADSR_OFF;
            }
        }
        break;
    }
    case PS1_ADSR_OFF:
    default:
        v->env_level = 0;
        v->active = 0;
        break;
    }
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
    spu->reverb_enabled = 0;
    spu->reverb_feedback = 64;
    spu->reverb_depth_left = 64;
    spu->reverb_depth_right = 64;
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

    memcpy(&v->num_progs, header + 18, 2);
    memcpy(&v->num_tones, header + 20, 2);
    memcpy(&v->num_vags,  header + 22, 2);
    if (v->num_progs > 128 || v->num_vags > 255) return PS1_VAB_NONE;

    /* VAG offset table location:
     * Header layout:
     *   32 bytes (VabHdr)
     * + 2048 bytes (128 ProgAtr)
     * + (16 * num_progs * 32 bytes) (VagAtr)
     * Followed by uint16_t vag_offsets[256].
     */
    uint32_t tone_count = (uint32_t)v->num_progs * 16u;
    uint32_t offtbl_pos = 32u + 2048u + tone_count * 32u;
    if (offtbl_pos + 512u > header_size) return PS1_VAB_NONE;
    if (body == NULL) {
        v->body = header + offtbl_pos + 512u;
        v->body_size = header_size - offtbl_pos - 512u;
    }
    unsigned packed_program = 0;
    for (unsigned program = 0; program < 128; ++program) {
        unsigned tones = header[32u + program * 16u];
        if (tones > 16) return PS1_VAB_NONE;
        v->program_tone_counts[program] = (uint8_t)tones;
        if (tones != 0) {
            if (packed_program >= v->num_progs) return PS1_VAB_NONE;
            v->program_tone_offsets[program] = 0x820u + packed_program++ * 512u;
        }
    }
    if (packed_program != v->num_progs) return PS1_VAB_NONE;
    {
        const uint16_t *offtbl = (const uint16_t *)(const void *)(header + offtbl_pos);
        uint32_t cum_offset = (uint32_t)offtbl[0] * 8u;
        for (uint32_t i = 1; i <= v->num_vags && i < 256; i++) {
            v->vag_body_offsets[i] = cum_offset;
            v->vag_lengths[i] = (uint32_t)offtbl[i] * 8u;
            if (cum_offset > v->body_size || v->vag_lengths[i] > v->body_size - cum_offset) return PS1_VAB_NONE;
            cum_offset += v->vag_lengths[i];
        }
    }
    v->loaded = 1;
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
    voice_init_adsr(v, 0, 0);

    v->curr_sample = fetch_voice_raw_sample(v);
    v->next_sample = fetch_voice_raw_sample(v);
}

void ps1_spu_key_on_vab(Ps1Spu *spu, int voice_idx, Ps1VabId vab_id,
                        int prog, int tone, int note, int fine,
                        int voll, int volr) {
    if (voice_idx < 0 || voice_idx >= PS1_SPU_VOICES) return;
    if (vab_id < 0 || vab_id >= PS1_SPU_VAB_SLOTS || !spu->vabs[vab_id].loaded) return;

    Ps1VabSlot *slot = &spu->vabs[vab_id];
    if (prog < 0 || prog >= 128 || tone < 0 || tone >= slot->program_tone_counts[prog]) return;

    /* VagAtr offset */
    uint32_t vag_atr_offset = slot->program_tone_offsets[prog] + (uint32_t)tone * 32u;
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
    double semitones = (double)(note - (int)center_note)
        + (double)fine / 100.0 - (double)atr[5] / 128.0;
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
    v->sample_len = slot->vag_lengths[vag_idx];
    v->block_sample_idx = ADPCM_SAMPLES_PER_BLOCK;
    v->pitch_step = pitch;
    v->vol_left = (int16_t)vl;
    v->vol_right = (int16_t)vr;

    uint16_t adsr1 = 0;
    uint16_t adsr2 = 0;
    memcpy(&adsr1, atr + 16, 2);
    memcpy(&adsr2, atr + 18, 2);
    voice_init_adsr(v, adsr1, adsr2);

    v->curr_sample = fetch_voice_raw_sample(v);
    v->next_sample = fetch_voice_raw_sample(v);
}

void ps1_spu_voice_set_adsr(Ps1Spu *spu, int voice_idx, uint16_t adsr1, uint16_t adsr2) {
    if (voice_idx < 0 || voice_idx >= PS1_SPU_VOICES) return;
    voice_init_adsr(&spu->voices[voice_idx], adsr1, adsr2);
}

void ps1_spu_key_off(Ps1Spu *spu, int voice_idx) {
    if (voice_idx < 0 || voice_idx >= PS1_SPU_VOICES) return;
    Ps1Voice *v = &spu->voices[voice_idx];
    if (!v->active) return;
    if (v->adsr_release_rate > 0 && v->env_phase != PS1_ADSR_RELEASE) {
        v->env_phase = PS1_ADSR_RELEASE;
    } else {
        v->active = 0;
        v->env_phase = PS1_ADSR_OFF;
        v->env_level = 0;
    }
}

void ps1_spu_key_off_all(Ps1Spu *spu) {
    for (int v = 0; v < PS1_SPU_VOICES; v++) {
        spu->voices[v].active = 0;
        spu->voices[v].env_phase = PS1_ADSR_OFF;
        spu->voices[v].env_level = 0;
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

void ps1_spu_set_reverb_enabled(Ps1Spu *spu, int enabled) {
    if (!spu) return;
    spu->reverb_enabled = enabled ? 1 : 0;
}

void ps1_spu_set_reverb_params(Ps1Spu *spu, int type, int depth_l, int depth_r,
                               int vol_l, int vol_r, int feedback, int delay) {
    if (!spu) return;
    spu->reverb_type = (int16_t)type;
    spu->reverb_depth_left = (int16_t)depth_l;
    spu->reverb_depth_right = (int16_t)depth_r;
    spu->reverb_vol_left = (int16_t)vol_l;
    spu->reverb_vol_right = (int16_t)vol_r;
    spu->reverb_feedback = (int16_t)feedback;
    spu->reverb_delay = (int16_t)delay;
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
            int32_t samp = (int32_t)voice->curr_sample + (int32_t)(((int64_t)diff * frac) >> 16);

            /* ADSR envelope scaling */
            samp = (int32_t)(((int64_t)samp * voice->env_level) >> 15);

            mix_l += (samp * (int32_t)voice->vol_left) >> 15;
            mix_r += (samp * (int32_t)voice->vol_right) >> 15;

            voice_step_adsr(voice);

            voice->frac_pos += voice->pitch_step;
            while (voice->frac_pos >= 0x10000 && voice->active) {
                voice->curr_sample = voice->next_sample;
                if (voice->source_ended) {
                    voice->active = 0;
                    voice->env_phase = PS1_ADSR_OFF;
                    voice->env_level = 0;
                    voice->next_sample = 0;
                } else voice->next_sample = fetch_voice_raw_sample(voice);
                voice->frac_pos -= 0x10000;
            }
        }

        /* Reverb DSP */
        if (spu->reverb_enabled) {
            uint32_t rpos = spu->reverb_pos;
            uint32_t delay_base = spu->reverb_delay ? ((uint32_t)spu->reverb_delay * 32u) : 2048u;
            if (delay_base > 4000u) delay_base = 4000u;
            uint32_t tap1 = (rpos + PS1_SPU_REVERB_BUF_SIZE - delay_base) % PS1_SPU_REVERB_BUF_SIZE;
            uint32_t tap2 = (rpos + PS1_SPU_REVERB_BUF_SIZE - (delay_base * 3u / 2u)) % PS1_SPU_REVERB_BUF_SIZE;

            int32_t delayed_l = ((int32_t)spu->reverb_ring[tap1][0] * 3 + (int32_t)spu->reverb_ring[tap2][0] * 2) / 5;
            int32_t delayed_r = ((int32_t)spu->reverb_ring[tap1][1] * 3 + (int32_t)spu->reverb_ring[tap2][1] * 2) / 5;

            int32_t fb = spu->reverb_feedback;
            if (fb < 0) fb = 0;
            if (fb > 127) fb = 127;

            int32_t in_l = mix_l + ((delayed_l * fb) / 128);
            int32_t in_r = mix_r + ((delayed_r * fb) / 128);
            if (in_l > 32767) in_l = 32767;
            if (in_l < -32768) in_l = -32768;
            if (in_r > 32767) in_r = 32767;
            if (in_r < -32768) in_r = -32768;

            spu->reverb_ring[rpos][0] = (int16_t)in_l;
            spu->reverb_ring[rpos][1] = (int16_t)in_r;
            spu->reverb_pos = (rpos + 1) % PS1_SPU_REVERB_BUF_SIZE;

            int32_t rdepth_l = spu->reverb_depth_left;
            int32_t rdepth_r = spu->reverb_depth_right;
            mix_l += (delayed_l * rdepth_l) / 128;
            mix_r += (delayed_r * rdepth_r) / 128;
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

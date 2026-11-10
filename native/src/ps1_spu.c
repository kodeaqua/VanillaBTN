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
    v->adsr_attack_mode = (adsr1 >> 15) & 1;
    v->adsr_attack_rate = (adsr1 >> 8) & 0x7F;
    v->adsr_decay_rate = (adsr1 >> 4) & 0x0F;
    int sl_raw = adsr1 & 0x0F;
    v->adsr_sustain_level = (sl_raw + 1) * 0x800;
    v->adsr_sustain_mode = (adsr2 >> 15) & 1;
    v->adsr_sustain_dir = (adsr2 >> 14) & 1;
    v->adsr_sustain_rate = (adsr2 >> 6) & 0x7F;
    v->adsr_release_mode = (adsr2 >> 5) & 1;
    v->adsr_release_rate = adsr2 & 0x1F;

    v->env_counter = 0;
    v->env_phase = PS1_ADSR_ATTACK;
    v->env_level = 0;
}

/* PS1 SPU shift/step counters advance at 44.1 kHz, independently of pitch.
 * https://psx-spx.consoledev.net/soundprocessingunitspu/
 * All-ones rates deliberately leave the counter stopped. */
static void voice_step_adsr(Ps1Voice *v) {
    if (!v->active) return;
    int shift, step_value = 0, decrease = 0, exponential = 0, frozen;
    switch (v->env_phase) {
    case PS1_ADSR_ATTACK:
        shift = v->adsr_attack_rate >> 2;
        step_value = v->adsr_attack_rate & 3;
        exponential = v->adsr_attack_mode;
        frozen = v->adsr_attack_rate == 127;
        break;
    case PS1_ADSR_DECAY:
        shift = v->adsr_decay_rate;
        decrease = exponential = 1;
        frozen = 0;
        break;
    case PS1_ADSR_SUSTAIN:
        shift = v->adsr_sustain_rate >> 2;
        step_value = v->adsr_sustain_rate & 3;
        decrease = v->adsr_sustain_dir;
        exponential = v->adsr_sustain_mode;
        frozen = v->adsr_sustain_rate == 127;
        break;
    case PS1_ADSR_RELEASE:
        shift = v->adsr_release_rate;
        decrease = 1;
        exponential = v->adsr_release_mode;
        frozen = v->adsr_release_rate == 31;
        break;
    default:
        return;
    }

    int32_t step = 7 - step_value;
    if (decrease) step = ~step;
    /* Multiply rather than left-shifting a negative signed value. */
    if (shift < 11) step *= 1 << (11 - shift);
    uint32_t increment = 0x8000u >> (shift > 11 ? shift - 11 : 0);
    if (exponential && !decrease && v->env_level > 0x6000) {
        if (shift < 10) {
            step >>= 2;
        } else if (shift >= 11) {
            increment >>= 2;
        } else {
            step >>= 1;
            increment >>= 1;
        }
    } else if (exponential && decrease) {
        step = (step * v->env_level) >> 15;
    }
    if (!frozen && increment == 0) increment = 1;
    v->env_counter += increment;
    if (!(v->env_counter & 0x8000u)) return;
    v->env_counter = 0;
    v->env_level += step;
    if (v->env_level < 0) v->env_level = 0;
    if (v->env_level > 0x7FFF) v->env_level = 0x7FFF;

    if (v->env_phase == PS1_ADSR_ATTACK && v->env_level == 0x7FFF) {
        v->env_phase = PS1_ADSR_DECAY;
    } else if (v->env_phase == PS1_ADSR_DECAY && v->env_level <= v->adsr_sustain_level) {
        v->env_phase = PS1_ADSR_SUSTAIN;
    } else if (v->env_phase == PS1_ADSR_RELEASE && v->env_level == 0) {
        v->env_phase = PS1_ADSR_OFF;
        v->active = 0;
    }
}

/* -------------------------------------------------------------------------
 * SPU Lifecycle
 * ---------------------------------------------------------------------- */
void ps1_spu_init(Ps1Spu *spu) {
    if (spu == NULL) return;
    memset(spu, 0, sizeof(*spu));
    spu->master_vol_left = 0x3FFF;
    spu->master_vol_right = 0x3FFF;
    spu->cd_vol_left = 0x1FFF;
    spu->cd_vol_right = 0x1FFF;
    ps1_reverb_set_preset(&spu->reverb, 0, 1);
    spu->reverb_enabled = 0;
    spu->reverb_feedback = 64;
    spu->reverb_depth_left = 64;
    spu->reverb_depth_right = 64;
}

Ps1VabId ps1_spu_load_vab(Ps1Spu *spu, const uint8_t *header, uint32_t header_size,
                          const uint8_t *body, uint32_t body_size) {
    if (!spu || !header || header_size < 32) return PS1_VAB_NONE;

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
    if (!spu || id < 0 || id >= PS1_SPU_VAB_SLOTS) return;
    for (int v = 0; v < PS1_SPU_VOICES; v++) {
        if (spu->voices[v].vab_id == id) {
            memset(&spu->voices[v], 0, sizeof(spu->voices[v]));
            spu->reverb_voice_mask &= ~(1u << v);
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
    if (!spu || voice_idx < 0 || voice_idx >= PS1_SPU_VOICES || !data || data_len < ADPCM_BLOCK_SIZE) return;

    spu->reverb_voice_mask &= ~(1u << voice_idx);
    Ps1Voice *v = &spu->voices[voice_idx];
    memset(v, 0, sizeof(*v));
    v->active = 1;
    v->vab_id = PS1_VAB_NONE;
    v->sample_data = data;
    v->sample_len = data_len;
    v->block_sample_idx = ADPCM_SAMPLES_PER_BLOCK;
    v->pitch_step = pitch_16_16 ? pitch_16_16 : 0x10000;
    if (v->pitch_step > 0x40000) v->pitch_step = 0x40000;
    v->vol_left = vol_left;
    v->vol_right = vol_right;
    /* Raw sample playback has no VAB ADSR registers: hold its explicit volume.
     * VAB voices, including zero-valued registers, always use the envelope. */
    voice_init_adsr(v, 0, 127 << 6);
    v->env_phase = PS1_ADSR_SUSTAIN;
    v->env_level = 0x7FFF;

    v->curr_sample = fetch_voice_raw_sample(v);
    v->next_sample = fetch_voice_raw_sample(v);
}

void ps1_spu_key_on_vab(Ps1Spu *spu, int voice_idx, Ps1VabId vab_id,
                        int prog, int tone, int note, int fine,
                        int voll, int volr) {
    if (!spu || voice_idx < 0 || voice_idx >= PS1_SPU_VOICES) return;
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
    /* PS1 SPU pitch register is 14 bits (0..0x3FFF), max ~4.0x sample rate.
     * Clamp 16.16 pitch step to 0x40000 to prevent runaway interpolation. */
    if (pitch > 0x40000) pitch = 0x40000;

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
    ps1_spu_set_reverb_voice(spu, 1u << voice_idx, (atr[1] & 4) != 0);

    v->curr_sample = fetch_voice_raw_sample(v);
    v->next_sample = fetch_voice_raw_sample(v);
}

void ps1_spu_voice_set_adsr(Ps1Spu *spu, int voice_idx, uint16_t adsr1, uint16_t adsr2) {
    if (!spu || voice_idx < 0 || voice_idx >= PS1_SPU_VOICES) return;
    voice_init_adsr(&spu->voices[voice_idx], adsr1, adsr2);
}

void ps1_spu_key_off(Ps1Spu *spu, int voice_idx) {
    if (!spu || voice_idx < 0 || voice_idx >= PS1_SPU_VOICES) return;
    Ps1Voice *v = &spu->voices[voice_idx];
    if (!v->active) return;
    if (v->env_phase != PS1_ADSR_RELEASE) {
        v->env_phase = PS1_ADSR_RELEASE;
        v->env_counter = 0;
    }
}

void ps1_spu_key_off_all(Ps1Spu *spu) {
    if (!spu) return;
    for (int v = 0; v < PS1_SPU_VOICES; v++) {
        spu->voices[v].active = 0;
        spu->voices[v].env_phase = PS1_ADSR_OFF;
        spu->voices[v].env_level = 0;
    }
}

int ps1_spu_voice_is_active(const Ps1Spu *spu, int voice_idx) {
    if (!spu || voice_idx < 0 || voice_idx >= PS1_SPU_VOICES) return 0;
    return spu->voices[voice_idx].active;
}

void ps1_spu_set_master_vol(Ps1Spu *spu, int16_t left, int16_t right) {
    if (!spu) return;
    spu->master_vol_left = left;
    spu->master_vol_right = right;
}

void ps1_spu_set_cd_vol(Ps1Spu *spu, int16_t left, int16_t right) {
    if (!spu) return;
    spu->cd_vol_left = left;
    spu->cd_vol_right = right;
}

void ps1_spu_set_reverb_enabled(Ps1Spu *spu, int enabled) {
    if (!spu) return;
    spu->reverb_enabled = enabled ? 1 : 0;
}

void ps1_spu_set_reverb_params(Ps1Spu *spu, int type, int depth_l, int depth_r,
                               int vol_l, int vol_r, int feedback, int delay) {
    if (!spu || type < -9 || type > 9) return;
    int clear = type < 0;
    if (clear) type = -type;
    if (spu->reverb_type != type || clear) {
        ps1_reverb_set_preset(&spu->reverb, type, clear);
    }
    spu->reverb_type = (int16_t)type;
    spu->reverb_depth_left = (int16_t)depth_l;
    spu->reverb_depth_right = (int16_t)depth_r;
    spu->reverb_vol_left = (int16_t)vol_l;
    spu->reverb_vol_right = (int16_t)vol_r;
    spu->reverb_feedback = (int16_t)feedback;
    spu->reverb_delay = (int16_t)delay;
    if (depth_l < -127) depth_l = -127;
    if (depth_l > 127) depth_l = 127;
    if (depth_r < -127) depth_r = -127;
    if (depth_r > 127) depth_r = 127;
    spu->reverb_gain_left = (int16_t)(depth_l * 32767 / 127);
    spu->reverb_gain_right = (int16_t)(depth_r * 32767 / 127);
    ps1_reverb_set_echo(&spu->reverb, type, delay, feedback);
}

uint32_t ps1_spu_set_reverb_voice(Ps1Spu *spu, uint32_t mask, int enabled) {
    if (!spu) return 0;
    mask &= 0x00ffffff;
    if (enabled) spu->reverb_voice_mask |= mask;
    else spu->reverb_voice_mask &= ~mask;
    return spu->reverb_voice_mask;
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
        int32_t send_l = 0, send_r = 0;

        for (int v = 0; v < PS1_SPU_VOICES; v++) {
            Ps1Voice *voice = &spu->voices[v];
            if (!voice->active) continue;

            /* Linear interpolation */
            int32_t frac = (int32_t)(voice->frac_pos & 0xFFFF);
            int32_t diff = (int32_t)voice->next_sample - (int32_t)voice->curr_sample;
            int32_t samp = (int32_t)voice->curr_sample + (int32_t)(((int64_t)diff * frac) >> 16);

            /* ADSR envelope scaling */
            samp = (int32_t)(((int64_t)samp * voice->env_level) >> 15);

            int32_t left = (samp * (int32_t)voice->vol_left) >> 15;
            int32_t right = (samp * (int32_t)voice->vol_right) >> 15;
            mix_l += left;
            mix_r += right;
            if (spu->reverb_voice_mask & (1u << v)) {
                send_l += left;
                send_r += right;
            }

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

        if (spu->mono_voices) {
            mix_l = mix_r = (mix_l + mix_r) / 2;
            send_l = send_r = (send_l + send_r) / 2;
        }
        if (spu->cd_input && spu->cd_input_position < spu->cd_input_frames) {
            unsigned index = spu->cd_input_position++ * 2;
            int left = (spu->cd_input[index] * (int32_t)spu->cd_vol_left) >> 15;
            int right = (spu->cd_input[index + 1] * (int32_t)spu->cd_vol_right) >> 15;
            if (spu->cd_mix) { mix_l += left; mix_r += right; }
            if (spu->cd_reverb) { send_l += left; send_r += right; }
        }
        int16_t wet[2];
        ps1_reverb_clock(&spu->reverb, send_l, send_r, spu->reverb_enabled, wet);
        mix_l += (wet[0] * (int32_t)spu->reverb_gain_left) >> 15;
        mix_r += (wet[1] * (int32_t)spu->reverb_gain_right) >> 15;

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
